//
//  KextsPatcher.cpp
//  KextsPatcher
//
//  Copyright © 2018 Vanilla. All rights reserved.
//

#include <plugin_start.hpp>
#include <Headers/kern_api.hpp>
#include <Headers/kern_util.hpp>

bool PRODUCT_NAME::init(OSDictionary *dict)
{
    bool res = IOService::init(dict);
    KextsPatcherPlugin *kextsPatcherPlugin = new KextsPatcherPlugin();
    kextsPatcherPlugin->init(dict);
    return res;
}


KernelPatcher::KextInfo *kextList;
size_t kextListSize;

KextPatch *patchesMod;
size_t patchesModSize;

static KextsPatcherPlugin *callbackKextsPatcher = nullptr;

bool KextsPatcherPlugin::init(OSDictionary *dict)
{
    OSObject *object = dict->getObject("KextsConfig");
    auto kextConfig = OSDynamicCast(OSDictionary, object);
    
    DBGLOG("KextsPatcher", "init");
    this->getKextConfig(kextConfig);
    
    callbackKextsPatcher = this;
    
    LiluAPI::Error error = lilu.onKextLoad(kextList, kextListSize, [](void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
        callbackKextsPatcher->processKext(patcher, index, address, size);
    }, this);
    
    if (error != LiluAPI::Error::NoError) {
        SYSLOG("init", "failed to register onKextLoad method %d", error);
        return false;
    }
    
    return true;
}

void KextsPatcherPlugin::processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size)
{
    for (size_t i = 0; i < kextListSize; i++) {
        if (kextList[i].loadIndex == index) {
            if (progressState[i] != ProcessingState::EverythingDone) {
                DBGLOG("KextsPatcher", "processKext patchesModSize:%zu",  patchesModSize);
                applyPatches(patcher, index, patchesMod, patchesModSize);
                
                progressState[i] |= ProcessingState::ControllersLoaded;
                break;
            }
        }
    }
    
    DBGLOG("KextsPatcher", "processKext kextListSize:%zu  index : %zu",kextListSize, index);
    
    patcher.clearError();
}

void KextsPatcherPlugin::applyPatches(KernelPatcher &patcher, size_t index, const KextPatch *patches, size_t patchNum) {
    DBGLOG("KextsPatcher", "applying patches for %lu kext", index);
    for (size_t p = 0; p < patchNum; p++) {
        auto &patch = patches[p];
        if (patch.patch.kext->loadIndex == index) {
            if (patcher.compatibleKernel(patch.minKernel, patch.maxKernel)) {
                DBGLOG("KextsPatcher", "applying %lu patch for %lu kext", p, index);
                patcher.applyLookupPatch(&patch.patch);
                // Do not really care for the errors for now
                patcher.clearError();
            }
        }
    }
}


void KextsPatcherPlugin::getKextConfig(OSDictionary *kextConfig)
{
    if (kextConfig) {
        auto kexts = OSDynamicCast(OSDictionary, kextConfig->getObject("Kexts"));
        this->generateKexts(kexts);
        
        auto patches = OSDynamicCast(OSArray, kextConfig->getObject("Patches"));
        this->generatePatches(patches);
    }
}

void KextsPatcherPlugin::generatePatches(OSArray *patches) {
    if (patches) {
        DBGLOG("KextsPatcher", "generatePatches");
        
        patchesModSize = patches->getCount();
        DBGLOG("KextsPatcher", "generatePatches patchesSize:%zu", patchesModSize);
        patchesMod = new KextPatch[patchesModSize];
        
        for (uint patchesModIndex = 0; patchesModIndex < patchesModSize; patchesModIndex++) {
            auto patches_info = OSDynamicCast(OSDictionary, patches->getObject(patchesModIndex));
            auto Count = OSDynamicCast(OSNumber, patches_info->getObject("Count"));
            auto Find = OSDynamicCast(OSData, patches_info->getObject("Find"));
            auto MaxKernel = OSDynamicCast(OSNumber, patches_info->getObject("MaxKernel"));
            auto MinKernel = OSDynamicCast(OSNumber, patches_info->getObject("MinKernel"));
            auto patchesName = OSDynamicCast(OSString, patches_info->getObject("Name"));
            auto Replace = OSDynamicCast(OSData, patches_info->getObject("Replace"));
            
            auto FindBuf = static_cast<const uint8_t *>(Find->getBytesNoCopy());
            auto ReplaceBuf = static_cast<const uint8_t *>(Replace->getBytesNoCopy());
            
            uint index = searchKextIndex(patchesName->getCStringNoCopy());
            DBGLOG("KextsPatcher", "generatePatches &kextList[index]:%s, index:%d", kextList[index].id, index);
            
            KernelPatcher::LookupPatch patch = {&kextList[index], FindBuf, ReplaceBuf, Find->getLength(), Count->unsigned32BitValue()};
            
            patchesMod[patchesModIndex] = {patch, MinKernel ? MinKernel->unsigned32BitValue() : KernelPatcher::KernelAny, MaxKernel? MaxKernel->unsigned32BitValue():KernelPatcher::KernelAny};
            
            DBGLOG("KextsPatcher", "generatePatches { %s}",patchesName->getCStringNoCopy());
        }
    }
}

uint KextsPatcherPlugin::searchKextIndex(const char *keyName)
{
    uint kextIndexsSize = kextIndexs->getCount();
    for (int kextIndex =0; kextIndex < kextIndexsSize; kextIndex++) {
        OSObject *object = kextIndexs->getObject(kextIndex);
        auto key = OSDynamicCast(OSSymbol, object);
        const char *value =key->getCStringNoCopy();
        
        if (strcmp(keyName, value) == 0) {
            return kextIndex;
        }
    }
    
    return -1;
}

void KextsPatcherPlugin::generateKexts(OSDictionary *kexts)
{
    if (kexts) {
        OSCollectionIterator *iter = OSCollectionIterator::withCollection(kexts);
        if (iter)
        {
            DBGLOG("KextsPatcher", "generateKexts");
            OSObject *object = NULL;
            
            kextListSize = kexts->getCount();
            DBGLOG("KextsPatcher", "generateKexts kextListSize:%zu",kextListSize);
            kextList = new KernelPatcher::KextInfo[kextListSize];
            
            kextIndexs = new OSArray;
            kextIndexs->initWithCapacity((uint)kextListSize);
            
            size_t kextListIndex = 0;
            
            progressState = new int[kextListSize];
            
            while ((object = iter->getNextObject()))
            {
                progressState[kextListIndex] = ProcessingState::NotReady;
                
                auto key = OSDynamicCast(OSSymbol, object);
                const char *keyName =key->getCStringNoCopy();
                DBGLOG("KextsPatcher", "generateKexts key:%s",keyName);
                
                kextIndexs->setObject(object);
                
                auto kextInfo =OSDynamicCast(OSDictionary, kexts->getObject(key));
                auto kextID = OSDynamicCast(OSString,kextInfo->getObject("Id"));
                auto kextPaths = OSDynamicCast(OSArray, kextInfo->getObject("Paths"));
                
                uint pathsTotal = kextPaths->getCount();
                const char ** kextPath = new const char*[pathsTotal];
                for (uint paths_i = 0; paths_i < pathsTotal; paths_i++) {
                    auto str = OSDynamicCast(OSString, kextPaths->getObject(paths_i));
                    kextPath[paths_i] = str->getCStringNoCopy();
                }
                
                auto Loaded = OSDynamicCast(OSBoolean, kextInfo->getObject("Loaded"));
                auto Reloadable = OSDynamicCast(OSBoolean, kextInfo->getObject("Reloadable"));
                auto Detect = OSDynamicCast(OSBoolean, kextInfo->getObject("Detect"));
                
                kextList[kextListIndex++] = { kextID->getCStringNoCopy(), kextPath, pathsTotal, {Loaded?Loaded->getValue():false, Reloadable?Reloadable->getValue():false}, {Detect?Detect->getValue():false}, KernelPatcher::KextInfo::Unloaded };
                
                DBGLOG("KextsPatcher", "KextInfo { %s}",kextList[kextListIndex-1].id);
                
            }
        }
    }
}

