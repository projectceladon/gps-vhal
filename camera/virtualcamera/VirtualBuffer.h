#ifndef HW_EMULATOR_CAMERA_VIRTUALD_CAMERA_FACTORY_H_K
#define HW_EMULATOR_CAMERA_VIRTUALD_CAMERA_FACTORY_H_K

#define MAX_CLIENT_BUF 8
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

        struct VideoBuffer clientBuf[8];
	unsigned int clientRevCount;
        unsigned int clientUsedCount;
        static ClientVideoBuffer* getClientInstance() {
	    if(ic_instance == NULL) {
                ic_instance = new ClientVideoBuffer();
            }
            return ic_instance;
        }

        ClientVideoBuffer() {
	    for(int i = 0; i < 8; i++) {
                clientBuf[i].buffer = (uint8_t *)malloc(460800);
            }
            clientRevCount = 0;
            clientUsedCount = 0;
       }
};

};

#endif // HW_EMULATOR_CAMERA_VIRTUALD_CAMERA_FACTORY_H_K
