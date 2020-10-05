#ifndef HW_EMULATOR_CAMERA_VIRTUALD_CAMERA_FACTORY_H_K
#define HW_EMULATOR_CAMERA_VIRTUALD_CAMERA_FACTORY_H_K

#include "VirtualCameraFactory.h"
#define FRAME_240P 320 * 240 * 1.5
#define FRAME_480P 640 * 480 * 1.5
#define FRAME_720P 1280 * 720 * 1.5
namespace android
{

enum class VideoBufferType {
  kI420,
  kARGB,
};


struct Resolution{
	int width;
	int height;
};
/// Video buffer and its information
struct VideoBuffer {
  /// Video buffer
  uint8_t* buffer;
  /// Resolution for the Video buffer
  Resolution resolution;
  // Buffer type
  VideoBufferType type;
  ~VideoBuffer() { delete[] buffer; }
};

class ClientVideoBuffer {

    public:
        static ClientVideoBuffer *ic_instance;

        struct VideoBuffer clientBuf[1];
	unsigned int clientRevCount;
        unsigned int clientUsedCount;
        static ClientVideoBuffer* getClientInstance() {
	    if(ic_instance == NULL) {
                ic_instance = new ClientVideoBuffer();
            }
            return ic_instance;
        }

        ClientVideoBuffer() {
	    for(int i = 0; i < 1; i++) {
		if(gVirtualCameraFactory.getmWidth() == 640 && gVirtualCameraFactory.getmHeight() == 480)
                	clientBuf[i].buffer = (uint8_t *)malloc(FRAME_480P);
		else if(gVirtualCameraFactory.getmWidth() == 320 && gVirtualCameraFactory.getmHeight() == 240)
			clientBuf[i].buffer = (uint8_t *)malloc(FRAME_240P);
		else if(gVirtualCameraFactory.getmWidth() == 1280 && gVirtualCameraFactory.getmHeight() == 720)
			clientBuf[i].buffer = (uint8_t *)malloc(FRAME_720P);
		else
			clientBuf[i].buffer = (uint8_t *)malloc(FRAME_480P); //ToDo
            }
            clientRevCount = 0;
            clientUsedCount = 0;
       }
};

};

#endif // HW_EMULATOR_CAMERA_VIRTUALD_CAMERA_FACTORY_H_K
