#include <algorithm>
#include <exception>

#include "streaming/vpl/vpl_legacy_source_engine.hpp"
#include "streaming/vpl/vpl_utils.hpp"
#include "streaming/vpl/vpl_dx11_accel.hpp"

#include "logger.hpp"

#if (MFX_VERSION >= 2000)
    #include <vpl/mfxdispatcher.h>
#endif

namespace cv {
namespace gapi {
namespace wip {
/* UTILS */
mfxU32 GetSurfaceSize(mfxU32 FourCC, mfxU32 width, mfxU32 height) {
    mfxU32 nbytes = 0;

    switch (FourCC) {
        case MFX_FOURCC_I420:
        case MFX_FOURCC_NV12:
            nbytes = width * height + (width >> 1) * (height >> 1) + (width >> 1) * (height >> 1);
            break;
        case MFX_FOURCC_I010:
        case MFX_FOURCC_P010:
            nbytes = width * height + (width >> 1) * (height >> 1) + (width >> 1) * (height >> 1);
            nbytes *= 2;
            break;
        case MFX_FOURCC_RGB4:
            nbytes = width * height * 4;
            break;
        default:
            break;
    }

    return nbytes;
}

VPLAccelerationPolicy::surface_ptr_t create_surface_RGB4(mfxFrameInfo frameInfo,
                                                         void* out_buf_ptr,
                                                         size_t out_buf_ptr_offset,
                                                         size_t out_buf_size)
{
    mfxU8* buf = reinterpret_cast<mfxU8*>(out_buf_ptr);
    mfxU16 surfW = frameInfo.Width * 4;
    mfxU16 surfH = frameInfo.Height;
    (void)surfH;

    // TODO more intelligent check
    if (out_buf_size <= out_buf_ptr_offset) {
        GAPI_LOG_WARNING(nullptr, "Not enough buffer, ptr: " << out_buf_ptr <<
                                  ", size: " << out_buf_size <<
                                  ", offset: " << out_buf_ptr_offset <<
                                  ", W: " << surfW <<
                                  ", H: " << surfH);
        GAPI_Assert(false && "Invalid offset");
    }

    VPLAccelerationPolicy::surface_ptr_t ret(new mfxFrameSurface1);
    memset(ret.get(), 0, sizeof(mfxFrameSurface1));

    ret->Info = frameInfo;
    ret->Data.B = buf + out_buf_ptr_offset;
    ret->Data.G = ret->Data.B + 1;
    ret->Data.R = ret->Data.B + 2;
    ret->Data.A = ret->Data.B + 3;
    ret->Data.Pitch = surfW;

    return ret;
}

VPLAccelerationPolicy::surface_ptr_t create_surface_other(mfxFrameInfo frameInfo,
                                                          void* out_buf_ptr,
                                                          size_t out_buf_ptr_offset,
                                                          size_t out_buf_size)
{
    mfxU8* buf = reinterpret_cast<mfxU8*>(out_buf_ptr);
    mfxU16 surfH = frameInfo.Height;
    mfxU16 surfW = (frameInfo.FourCC == MFX_FOURCC_P010) ? frameInfo.Width * 2 : frameInfo.Width;
    
    // TODO more intelligent check
    if (out_buf_size <=
        out_buf_ptr_offset + (surfW * surfH) + ((surfW / 2) * (surfH / 2))) {
        GAPI_LOG_WARNING(nullptr, "Not enough buffer, ptr: " << out_buf_ptr <<
                                  ", size: " << out_buf_size <<
                                  ", offset: " << out_buf_ptr_offset <<
                                  ", W: " << surfW <<
                                  ", H: " << surfH);
        GAPI_Assert(false && "Invalid offset");
    }

    VPLAccelerationPolicy::surface_ptr_t ret(new mfxFrameSurface1);
    memset(ret.get(), 0, sizeof(mfxFrameSurface1));
    
    ret->Info = frameInfo;
    ret->Data.Y     = buf + out_buf_ptr_offset;
    ret->Data.U     = buf + out_buf_ptr_offset + (surfW * surfH);
    ret->Data.V     = ret->Data.U + ((surfW / 2) * (surfH / 2));
    ret->Data.Pitch = surfW;

    return ret;
}


    
LegacyDecodeSession::LegacyDecodeSession(mfxSession sess,
                                         DecoderParams&& decoder_param,
                                         VPLLegacyDecodeEngine::file_ptr&& source) :
    EngineSession(sess, std::move(decoder_param.stream)),
    mfx_decoder_param(std::move(decoder_param.param)),
    source_handle(std::move(source)),
    stop_processing(false),
    acceleration_policy(),
    procesing_surface_ptr(),
    output_surface_ptr()
{
}

void LegacyDecodeSession::swap_surface() {
    GAPI_Assert(acceleration_policy && "Empty acceleration_policy");
    auto old_locked = procesing_surface_ptr.lock();
    try {
        auto cand = acceleration_policy->get_free_surface(decoder_pool_id).lock();

        GAPI_LOG_DEBUG(nullptr, "[" << session << "] swap surface"
                                ", old: " << old_locked.get() <<
                                ", new: "<< cand.get());

        procesing_surface_ptr = cand;
    } catch (const std::exception& ex) {
        GAPI_LOG_WARNING(nullptr, "[" << session << "] error: " << ex.what() <<
                                   "Abort");
    }
}

void LegacyDecodeSession::init_surface_pool(VPLAccelerationPolicy::pool_key_t key) {
    GAPI_Assert(key && "Init decode pull with empty key");
    decoder_pool_id = key;

    // prepare working decode surface
    swap_surface();
}

VPLLegacyDecodeEngine::VPLLegacyDecodeEngine() {

    GAPI_LOG_INFO(nullptr, "Create Legacy Decode Engine");
    create_pipeline(
        // 1) Reade File
        [this] (EngineSession& sess) -> ExecutionStatus
        {
            LegacyDecodeSession &my_sess = static_cast<LegacyDecodeSession&>(sess);
            my_sess.last_status = ReadEncodedStream(my_sess.stream, my_sess.source_handle.get());
            if (my_sess.last_status != MFX_ERR_NONE) {
                my_sess.source_handle.reset(); //close source
            }
            return ExecutionStatus::Continue;
        },
        // 2) enqueue ASYNC decode
        [this] (EngineSession& sess) -> ExecutionStatus
        {
            LegacyDecodeSession &my_sess = static_cast<LegacyDecodeSession&>(sess);

            my_sess.last_status =
                    MFXVideoDECODE_DecodeFrameAsync(my_sess.session,
                                                    my_sess.last_status == MFX_ERR_NONE
                                                        ? &my_sess.stream
                                                        : nullptr, /* No more data to read, start decode draining mode*/
                                                    my_sess.procesing_surface_ptr.lock().get(),
                                                    &my_sess.output_surface_ptr,
                                                    &my_sess.sync);
            return ExecutionStatus::Continue;
        },
        // 3) Wait for ASYNC decode result
        [this] (EngineSession& sess) -> ExecutionStatus
        {
            if (sess.last_status == MFX_ERR_NONE) // Got 1 decoded frame
            {
                do {
                    sess.last_status = MFXVideoCORE_SyncOperation(sess.session, sess.sync, 100);
                    if (MFX_ERR_NONE == sess.last_status) {

                        LegacyDecodeSession& my_sess = static_cast<LegacyDecodeSession&>(sess);
                        on_frame_ready(my_sess);
                    }
                } while (sess.last_status == MFX_WRN_IN_EXECUTION);
            }
            return ExecutionStatus::Continue;
        },
        // 4) Falls back on generic status procesing
        [this] (EngineSession& sess) -> ExecutionStatus
        {
            return this->process_error(sess.last_status, static_cast<LegacyDecodeSession&>(sess));
        }
    );
}

void VPLLegacyDecodeEngine::initialize_session(mfxSession mfx_session,
                                         DecoderParams&& decoder_param,
                                         file_ptr&& source_handle,
                                         std::unique_ptr<VPLAccelerationPolicy>&& acceleration_policy)
{
    mfxFrameAllocRequest decRequest = {};
    // Query number required surfaces for decoder
    MFXVideoDECODE_QueryIOSurf(mfx_session, &decoder_param.param, &decRequest);

    // External (application) allocation of decode surfaces
    GAPI_LOG_DEBUG(nullptr, "Query IOSurf for session: " << mfx_session <<
                            ", mfxFrameAllocRequest.NumFrameSuggested: " << decRequest.NumFrameSuggested <<
                            ", mfxFrameAllocRequest.Type: " << decRequest.Type);

    mfxU32 singleSurfaceSize = GetSurfaceSize(decoder_param.param.mfx.FrameInfo.FourCC,
                                              decoder_param.param.mfx.FrameInfo.Width,
                                              decoder_param.param.mfx.FrameInfo.Height);
    if (!singleSurfaceSize) {
        throw std::runtime_error("Cannot determine surface size for: fourCC" +
                                 std::to_string(decoder_param.param.mfx.FrameInfo.FourCC) +
                                 ", width: " + std::to_string(decoder_param.param.mfx.FrameInfo.Width) +
                                 ", height: " + std::to_string(decoder_param.param.mfx.FrameInfo.Height));
    }

    const auto &frameInfo = decoder_param.param.mfx.FrameInfo;
    auto surface_creator =
            [&frameInfo] (void* out_buf_ptr, size_t out_buf_ptr_offset,
                          size_t out_buf_size) -> VPLAccelerationPolicy::surface_ptr_t {
                return (frameInfo.FourCC == MFX_FOURCC_RGB4) ?
                        create_surface_RGB4(frameInfo, out_buf_ptr, out_buf_ptr_offset,
                                            out_buf_size) :
                        create_surface_other(frameInfo, out_buf_ptr, out_buf_ptr_offset,
                                             out_buf_size);};

    //TODO Configure preallocation size (how many frames we can hold)
    const size_t preallocated_frames_count = 3;
    VPLAccelerationPolicy::pool_key_t decode_pool_key =
                acceleration_policy->create_surface_pool(decRequest.NumFrameSuggested * preallocated_frames_count,
                                                         singleSurfaceSize,
                                                         surface_creator);

    // create session
    std::shared_ptr<LegacyDecodeSession> sess_ptr =
                register_session<LegacyDecodeSession>(mfx_session,
                                                      std::move(decoder_param),
                                                      std::move(source_handle));

    // TODO Use common pool for all sessions ?
    sess_ptr->acceleration_policy = std::move(acceleration_policy);
    sess_ptr->init_surface_pool(decode_pool_key);
}

VPLProcessingEngine::ExecutionStatus VPLLegacyDecodeEngine::execute_op(operation_t& op, EngineSession& sess) {
    return op(sess);
}
    
void VPLLegacyDecodeEngine::on_frame_ready(LegacyDecodeSession& sess)
{
    mfxFrameInfo *info = &sess.output_surface_ptr->Info;
    mfxFrameData *data = &sess.output_surface_ptr->Data;
    size_t w = info->Width;
    size_t h = info->Height;
    size_t p = data->Pitch;


    GAPI_LOG_DEBUG(nullptr, "[" << sess.session << "], surface: " << sess.output_surface_ptr <<
                            ", w: " << w << ", h: " << h << ", p: " << p);
    
    // manage memory ownership rely on acceleration policy 
    auto frame_adapter = sess.acceleration_policy->create_frame_adapter(sess.output_surface_ptr);
    ready_frames.push(cv::MediaFrame(std::move(frame_adapter)));
}

VPLProcessingEngine::ExecutionStatus VPLLegacyDecodeEngine::process_error(mfxStatus status, LegacyDecodeSession& sess)
{
    GAPI_LOG_DEBUG(nullptr, "status: " << mfxstatus_to_string(status));

    switch (status) {
        case MFX_ERR_NONE:
            return ExecutionStatus::Continue; 
        case MFX_ERR_MORE_DATA: // The function requires more bitstream at input before decoding can proceed
            if (!sess.source_handle) {
                // No more data to drain from decoder, start encode draining mode
                return ExecutionStatus::Processed;
            }
            else
                return ExecutionStatus::Continue; // read more data
            break;
        case MFX_ERR_MORE_SURFACE:
        {
            // The function requires more frame surface at output before decoding can proceed.
            // This applies to external memory allocations and should not be expected for
            // a simple internal allocation case like this
            try {
                sess.swap_surface();
                return ExecutionStatus::Continue; 
            } catch (const std::exception& ex) {
                GAPI_LOG_WARNING(nullptr, "[" << sess.session << "] error: " << ex.what() <<
                                          "Abort");
            }
            break;
        }
        case MFX_ERR_DEVICE_LOST:
            // For non-CPU implementations,
            // Cleanup if device is lost
            break;
        case MFX_WRN_DEVICE_BUSY:
            // For non-CPU implementations,
            // Wait a few milliseconds then try again
            break;
        case MFX_WRN_VIDEO_PARAM_CHANGED:
            // The decoder detected a new sequence header in the bitstream.
            // Video parameters may have changed.
            // In external memory allocation case, might need to reallocate the output surface
            break;
        case MFX_ERR_INCOMPATIBLE_VIDEO_PARAM:
            // The function detected that video parameters provided by the application
            // are incompatible with initialization parameters.
            // The application should close the component and then reinitialize it
            break;
        case MFX_ERR_REALLOC_SURFACE:
            // Bigger surface_work required. May be returned only if
            // mfxInfoMFX::EnableReallocRequest was set to ON during initialization.
            // This applies to external memory allocations and should not be expected for
            // a simple internal allocation case like this
            break;
        default:
            GAPI_LOG_WARNING(nullptr, "Unknown status code: " << mfxstatus_to_string(status));
            break;
    }

    return ExecutionStatus::Failed;
}
} // namespace wip
} // namespace gapi
} // namespace cv
