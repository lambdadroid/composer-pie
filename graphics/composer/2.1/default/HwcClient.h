/*
 * Copyright 2016 The Android Open Source Project
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

#ifndef ANDROID_HARDWARE_GRAPHICS_COMPOSER_V2_1_HWC_CLIENT_H
#define ANDROID_HARDWARE_GRAPHICS_COMPOSER_V2_1_HWC_CLIENT_H

#include <mutex>
#include <unordered_map>
#include <vector>

#include "Hwc.h"
#include "IComposerCommandBuffer.h"

namespace android {
namespace hardware {
namespace graphics {
namespace composer {
namespace V2_1 {
namespace implementation {

class BufferClone {
public:
    BufferClone();
    BufferClone(BufferClone&& other);

    BufferClone(const BufferClone& other) = delete;
    BufferClone& operator=(const BufferClone& other) = delete;

    BufferClone& operator=(buffer_handle_t handle);
    ~BufferClone();

    operator buffer_handle_t() const { return mHandle; }

private:
    void clear();

    buffer_handle_t mHandle;
};

class HwcClient : public IComposerClient {
public:
    HwcClient(HwcHal& hal);
    virtual ~HwcClient();

    void onHotplug(Display display, IComposerCallback::Connection connected);
    void onRefresh(Display display);
    void onVsync(Display display, int64_t timestamp);

    // IComposerClient interface
    Return<void> registerCallback(
            const sp<IComposerCallback>& callback) override;
    Return<uint32_t> getMaxVirtualDisplayCount() override;
    Return<void> createVirtualDisplay(uint32_t width, uint32_t height,
            PixelFormat formatHint, uint32_t outputBufferSlotCount,
            createVirtualDisplay_cb hidl_cb) override;
    Return<Error> destroyVirtualDisplay(Display display) override;
    Return<void> createLayer(Display display, uint32_t bufferSlotCount,
            createLayer_cb hidl_cb) override;
    Return<Error> destroyLayer(Display display, Layer layer) override;
    Return<void> getActiveConfig(Display display,
            getActiveConfig_cb hidl_cb) override;
    Return<Error> getClientTargetSupport(Display display,
            uint32_t width, uint32_t height,
            PixelFormat format, Dataspace dataspace) override;
    Return<void> getColorModes(Display display,
            getColorModes_cb hidl_cb) override;
    Return<void> getDisplayAttribute(Display display,
            Config config, Attribute attribute,
            getDisplayAttribute_cb hidl_cb) override;
    Return<void> getDisplayConfigs(Display display,
            getDisplayConfigs_cb hidl_cb) override;
    Return<void> getDisplayName(Display display,
            getDisplayName_cb hidl_cb) override;
    Return<void> getDisplayType(Display display,
            getDisplayType_cb hidl_cb) override;
    Return<void> getDozeSupport(Display display,
            getDozeSupport_cb hidl_cb) override;
    Return<void> getHdrCapabilities(Display display,
            getHdrCapabilities_cb hidl_cb) override;
    Return<Error> setActiveConfig(Display display, Config config) override;
    Return<Error> setColorMode(Display display, ColorMode mode) override;
    Return<Error> setPowerMode(Display display, PowerMode mode) override;
    Return<Error> setVsyncEnabled(Display display, Vsync enabled) override;
    Return<Error> setClientTargetSlotCount(Display display,
            uint32_t clientTargetSlotCount) override;
    Return<Error> setInputCommandQueue(
            const MQDescriptorSync<uint32_t>& descriptor) override;
    Return<void> getOutputCommandQueue(
            getOutputCommandQueue_cb hidl_cb) override;
    Return<void> executeCommands(uint32_t inLength,
            const hidl_vec<hidl_handle>& inHandles,
            executeCommands_cb hidl_cb) override;

private:
    struct LayerBuffers {
        std::vector<BufferClone> Buffers;
        BufferClone SidebandStream;
    };

    struct DisplayData {
        bool IsVirtual;

        std::vector<BufferClone> ClientTargets;
        std::vector<BufferClone> OutputBuffers;

        std::unordered_map<Layer, LayerBuffers> Layers;

        DisplayData(bool isVirtual) : IsVirtual(isVirtual) {}
    };

    class CommandReader : public CommandReaderBase {
    public:
        CommandReader(HwcClient& client);
        Error parse();

    private:
        bool parseSelectDisplay(uint16_t length);
        bool parseSelectLayer(uint16_t length);
        bool parseSetColorTransform(uint16_t length);
        bool parseSetClientTarget(uint16_t length);
        bool parseSetOutputBuffer(uint16_t length);
        bool parseValidateDisplay(uint16_t length);
        bool parseAcceptDisplayChanges(uint16_t length);
        bool parsePresentDisplay(uint16_t length);
        bool parseSetLayerCursorPosition(uint16_t length);
        bool parseSetLayerBuffer(uint16_t length);
        bool parseSetLayerSurfaceDamage(uint16_t length);
        bool parseSetLayerBlendMode(uint16_t length);
        bool parseSetLayerColor(uint16_t length);
        bool parseSetLayerCompositionType(uint16_t length);
        bool parseSetLayerDataspace(uint16_t length);
        bool parseSetLayerDisplayFrame(uint16_t length);
        bool parseSetLayerPlaneAlpha(uint16_t length);
        bool parseSetLayerSidebandStream(uint16_t length);
        bool parseSetLayerSourceCrop(uint16_t length);
        bool parseSetLayerTransform(uint16_t length);
        bool parseSetLayerVisibleRegion(uint16_t length);
        bool parseSetLayerZOrder(uint16_t length);

        hwc_rect_t readRect();
        std::vector<hwc_rect_t> readRegion(size_t count);
        hwc_frect_t readFRect();

        enum class BufferCache {
            CLIENT_TARGETS,
            OUTPUT_BUFFERS,
            LAYER_BUFFERS,
            LAYER_SIDEBAND_STREAMS,
        };
        Error lookupBuffer(BufferCache cache, uint32_t slot,
                bool useCache, buffer_handle_t& handle);

        Error lookupLayerSidebandStream(buffer_handle_t& handle)
        {
            return lookupBuffer(BufferCache::LAYER_SIDEBAND_STREAMS,
                    0, false, handle);
        }

        HwcClient& mClient;
        HwcHal& mHal;
        CommandWriterBase& mWriter;

        Display mDisplay;
        Layer mLayer;
    };

    HwcHal& mHal;

    // 64KiB minus a small space for metadata such as read/write pointers
    static constexpr size_t kWriterInitialSize =
        64 * 1024 / sizeof(uint32_t) - 16;
    std::mutex mCommandMutex;
    CommandReader mReader;
    CommandWriterBase mWriter;

    sp<IComposerCallback> mCallback;

    std::mutex mDisplayDataMutex;
    std::unordered_map<Display, DisplayData> mDisplayData;
};

} // namespace implementation
} // namespace V2_1
} // namespace composer
} // namespace graphics
} // namespace hardware
} // namespace android

#endif  // ANDROID_HARDWARE_GRAPHICS_COMPOSER_V2_1_HWC_CLIENT_H
