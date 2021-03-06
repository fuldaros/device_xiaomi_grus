/*
 * Copyright (C) 2019 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <dlfcn.h>

#define LOG_TAG "vendor.lineage.livedisplay@2.0-service.xiaomi"

#include <android-base/logging.h>
#include <binder/ProcessState.h>
#include <hidl/HidlTransportSupport.h>

#include "AdaptiveBacklight.h"
#include "ColorEnhancement.h"
#include "DisplayModes.h"
#include "DisplayModesSDM.h"
#include "PictureAdjustment.h"
#include "SDMController.h"
#include "SunlightEnhancement.h"


using android::OK;
using android::sp;
using android::status_t;
using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;

using ::vendor::lineage::livedisplay::V2_0::IAdaptiveBacklight;
using ::vendor::lineage::livedisplay::V2_0::IColorEnhancement;
using ::vendor::lineage::livedisplay::V2_0::IDisplayModes;
using ::vendor::lineage::livedisplay::V2_0::IPictureAdjustment;
using ::vendor::lineage::livedisplay::V2_0::implementation::AdaptiveBacklight;
using ::vendor::lineage::livedisplay::V2_0::implementation::ColorEnhancement;
using ::vendor::lineage::livedisplay::V2_0::ISunlightEnhancement;
using ::vendor::lineage::livedisplay::V2_0::implementation::DisplayModes;
using ::vendor::lineage::livedisplay::V2_0::implementation::DisplayModesSDM;
using ::vendor::lineage::livedisplay::V2_0::implementation::PictureAdjustment;
using ::vendor::lineage::livedisplay::V2_0::implementation::SDMController;
using ::vendor::lineage::livedisplay::V2_0::implementation::SunlightEnhancement;


int main() {
    // Vendor backend
    std::shared_ptr<SDMController> controller;
    uint64_t cookie = 0;

    // HIDL frontend
    sp<AdaptiveBacklight> ab;
    sp<ColorEnhancement> ce;
    sp<DisplayModes> dm;
    sp<DisplayModesSDM> dms;
    sp<PictureAdjustment> pa;
    sp<SunlightEnhancement> se;

    status_t status = OK;

    android::ProcessState::initWithDriver("/dev/vndbinder");

    LOG(INFO) << "LiveDisplay HAL service is starting.";

    controller = std::make_shared<SDMController>();
    if (controller == nullptr) {
        LOG(ERROR) << "Failed to create SDMController";
        goto shutdown;
    }

    status = controller->init(&cookie, 0);
    if (status != OK) {
        LOG(ERROR) << "Failed to initialize SDMController";
        goto shutdown;
    }

    ab = new AdaptiveBacklight();
    if (ab == nullptr) {
        LOG(ERROR)
            << "Can not create an instance of LiveDisplay HAL AdaptiveBacklight Iface, exiting.";
        goto shutdown;
    }

    ce = new ColorEnhancement();
    if (ce == nullptr) {
        LOG(ERROR)
            << "Can not create an instance of LiveDisplay HAL ColorEnhancement Iface, exiting.";
        goto shutdown;
    }

    dm = new DisplayModes();
    if (dm == nullptr) {
        LOG(ERROR) << "Can not create an instance of LiveDisplay HAL DisplayModes Iface, exiting.";
        goto shutdown;
    }

    dms = new DisplayModesSDM(controller, cookie);
    if (dms == nullptr) {
        LOG(ERROR) << "Can not create an instance of LiveDisplay HAL DisplayModesSDM Iface, exiting.";
        goto shutdown;
    }

    pa = new PictureAdjustment(controller, cookie);
    if (pa == nullptr) {
        LOG(ERROR)
            << "Can not create an instance of LiveDisplay HAL PictureAdjustment Iface, exiting.";
        goto shutdown;
    }

    // SunlightEnhancement
    se = new SunlightEnhancement();
    if (se == nullptr) {
        LOG(ERROR)
            << "Can not create an instance of LiveDisplay HAL SunlightEnhancement Iface, exiting.";
        goto shutdown;
    }

    if (!dms->isSupported() && !pa->isSupported()) {
        // SDM backend isn't ready yet, so restart and try again

        goto shutdown;
    }


    configureRpcThreadpool(1, true /*callerWillJoin*/);

    if (ab->isSupported()) {
        status = ab->registerAsService();
        if (status != OK) {
            LOG(ERROR) << "Could not register service for LiveDisplay HAL AdaptiveBacklight Iface ("
                       << status << ")";
            goto shutdown;
        }
    }

    if (ce->isSupported()) {
        status = ce->registerAsService();
        if (status != OK) {
            LOG(ERROR) << "Could not register service for LiveDisplay HAL ColorEnhancement Iface ("
                       << status << ")";
            goto shutdown;
        }
    }

    // fallback to SDM impl if kernel display modes isn't supported
    if (dm->isSupported()) {
        status = dm->registerAsService();
        if (status != OK) {
            LOG(ERROR) << "Could not register service for LiveDisplay HAL DisplayModes Iface ("
                       << status << ")";
            goto shutdown;
        }
    } else if (dms->isSupported()) {
        status = dms->registerAsService();
        if (status != OK) {
            LOG(ERROR) << "Could not register service for LiveDisplay HAL DisplayModesSDM Iface ("
                       << status << ")";
            goto shutdown;
        }
    }

    if (pa->isSupported()) {
        status = pa->registerAsService();
        if (status != OK) {
            LOG(ERROR) << "Could not register service for LiveDisplay HAL PictureAdjustment Iface ("
                       << status << ")";
            goto shutdown;
        }
    }

    // SunlightEnhancement service
    if (se->isSupported()) {
        status = se->registerAsService();
        if (status != OK) {
            LOG(ERROR) << "Could not register service for LiveDisplay HAL SunlightEnhancement Iface ("
                       << status << ")";
            goto shutdown;
        }
    }

    LOG(INFO) << "LiveDisplay HAL service is ready.";
    joinRpcThreadpool();
    // Should not pass this line

shutdown:
    // Cleanup what we started
    controller->deinit(cookie, 0);

    // In normal operation, we don't expect the thread pool to shutdown
    LOG(ERROR) << "LiveDisplay HAL service is shutting down.";
    return 1;
}
