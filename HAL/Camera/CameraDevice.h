#pragma once

#include <HAL/Devices/SharedLoad.h>

#include <PbMsgs/Image.h>

#include <HAL/Camera/CameraDriverInterface.h>
#include <HAL/Devices/DeviceRegistry.h>
#include <HAL/Devices/DeviceFactory.h>
#include <HAL/Utils/Uri.h>

namespace hal {

///////////////////////////////////////////////////////////////////////////////
// Generic camera device
class Camera : public CameraDriverInterface
{
public:
    ///////////////////////////////////////////////////////////////
    Camera()
    {
    }

    ///////////////////////////////////////////////////////////////
    Camera(const std::string& uri)
        : m_uri(uri)
    {
        m_cam = DeviceRegistry<CameraDriverInterface>::I().Create(m_uri);
    }

    ///////////////////////////////////////////////////////////////
    ~Camera()
    {
        Clear();
    }

    ///////////////////////////////////////////////////////////////
    inline void Reset()
    {
        Clear();
        m_cam = DeviceRegistry<CameraDriverInterface>::I().Create(m_uri);
    }

    ///////////////////////////////////////////////////////////////
    inline void Clear()
    {
//        DeviceRegistry<CameraDriverInterface>::Destroy(m_cam);
        m_cam = nullptr;
    }

    ///////////////////////////////////////////////////////////////
    std::shared_ptr<CameraDriverInterface> GetInputDevice()
    {
        return m_cam->GetInputDevice();
    }

    ///////////////////////////////////////////////////////////////
    bool Capture( pb::CameraMsg& Images )
    {
        Images.Clear();
        return m_cam->Capture(Images);
    }

    ///////////////////////////////////////////////////////////////
    bool Capture( pb::ImageArray& Images )
    {
        return Capture( Images.Ref() );
    }

    ///////////////////////////////////////////////////////////////
    bool Capture(
            std::vector<cv::Mat>& vImages
            )
    {
        std::vector<pb::ImageInfoMsg> vImageInfo;
        return Capture( vImages, vImageInfo );
    }

    ///////////////////////////////////////////////////////////////
    bool Capture(
            std::vector<cv::Mat>& vImages,
            std::vector<pb::ImageInfoMsg>& vImageInfo
            )
    {
        static pb::ImageArray pbImages;
        bool bRes = Capture( pbImages.Ref() );
        vImages.resize( pbImages.Size() );
        vImageInfo.resize( pbImages.Size() );
        if( bRes ){
            for( size_t ii = 0; ii < pbImages.Size(); ii++ ){
                vImages[ii] = cv::Mat( pbImages[ii] );
                vImageInfo[ii] = pbImages[ii].GetInfo();
            }
        }
        return bRes;
    }

    ///////////////////////////////////////////////////////////////
    size_t NumChannels() const
    {
        return m_cam->NumChannels();
    }
    
    ///////////////////////////////////////////////////////////////
    size_t Width( size_t idx = 0 ) const
    {
        return m_cam->Width(idx);
    }
    
    ///////////////////////////////////////////////////////////////
    size_t Height( size_t idx = 0 ) const
    {
        return m_cam->Height(idx);        
    }
    
    ///////////////////////////////////////////////////////////////
    std::string GetDeviceProperty(const std::string& sProperty)
    {
        return m_cam->GetDeviceProperty(sProperty);
    }
    
    ///////////////////////////////////////////////////////////////
    /// Return raw camera driver pointer, optionally dynamic_cast'd
    /// to another type. Returns NULL if wrong type.
    /// e.g. RectifyDriver* rectify = camera.GetDriver<RectifyDriver>();
    ///      if(rectify) {...}
    template<typename CameraDriverType = CameraDriverInterface>
    CameraDriverType* GetDriver()
    {
        CameraDriverInterface* di = m_cam.get();
        return dynamic_cast<CameraDriverType*>(di);
    }

protected:
    hal::Uri m_uri;
    std::shared_ptr<CameraDriverInterface> m_cam;
};

}
