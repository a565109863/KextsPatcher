//
//  kern_start.cpp
//  KextsPatcher
//
//  Created by Zhong-Mac on 2018/6/24.
//  Copyright © 2018年 Zhong-Mac. All rights reserved.
//


#include <plugin_start.hpp>
#include <Headers/kern_api.hpp>




static const char *bootargOff[] {
    "-kpoff"
};

static const char *bootargDebug[] {
    "-kpdbg"
};

static const char *bootargBeta[] {
    "-kpbeta"
};

PluginConfiguration ADDPR(config) {
    xStringify(PRODUCT_NAME),
    parseModuleVersion(xStringify(MODULE_VERSION)),
    LiluAPI::AllowNormal | LiluAPI::AllowInstallerRecovery | LiluAPI::AllowSafeMode,
    bootargOff,
    arrsize(bootargOff),
    bootargDebug,
    arrsize(bootargDebug),
    bootargBeta,
    arrsize(bootargBeta),
    KernelVersion::MountainLion,
    KernelVersion::Monterey,
    []() {
    }
};
