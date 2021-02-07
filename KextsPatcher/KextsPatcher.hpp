//
//  KextsPatcher.hpp
//  KextsPatcher
//
//  Copyright © 2017 Vanilla. All rights reserved.
//

#ifndef kern_cpuf_hpp
#define kern_cpuf_hpp

#include <Headers/kern_patcher.hpp>
#include <IOKit/IOService.h>

#include <Headers/kern_iokit.hpp>

struct KextPatch {
    KernelPatcher::LookupPatch patch;
    uint32_t minKernel;
    uint32_t maxKernel;
};

extern KernelPatcher::KextInfo *kextList;
extern size_t kextListSize;

extern KextPatch *patchesMod;
extern size_t patchesModSize;

class KextsPatcherPlugin {
public:
    bool init(OSDictionary *dict);
    
private:
    OSArray *vendorIndexs;
    OSArray *kextIndexs;
    void getKextConfig(OSDictionary *dict);
    void generateKexts(OSDictionary *dict);
    uint searchKextIndex(const char *keyName);
    void generatePatches(OSArray *dict);
    
    void processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);
    void applyPatches(KernelPatcher &patcher, size_t index, const KextPatch *patches, size_t patchesNum);

    /**
     *  Current progress mask
     */
    struct ProcessingState {
        enum {
            NotReady = 0,
            ControllersLoaded = 1
        };
    };
    int *progressState;
};

#endif /* kern_cpuf_hpp */
