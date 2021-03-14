#ifndef HW_EMULATOR_CAMERA_VIRTUALD_CAMERA_FACTORY_H_K
#define HW_EMULATOR_CAMERA_VIRTUALD_CAMERA_FACTORY_H_K

#include <mutex>

#define MAX_CLIENT_BUF 8
namespace android {

extern bool gIsInFrameI420;
extern bool gIsInFrameH264;

enum class VideoBufferType {
    kI420,
    kARGB,
};

struct Resolution {
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

    void reset() {
        std::fill(buffer, buffer + 460800, 0);
        decoded = false;
    }
    bool decoded = false;
};

class ClientVideoBuffer {
public:
    static ClientVideoBuffer* ic_instance;

    struct VideoBuffer clientBuf[1];
    unsigned int clientRevCount = 0;
    unsigned int clientUsedCount = 0;

    size_t receivedFrameNo = 0;
    size_t decodedFrameNo = 0;

    static ClientVideoBuffer* getClientInstance() {
        if (ic_instance == NULL) {
            ic_instance = new ClientVideoBuffer();
        }
        return ic_instance;
    }

    ClientVideoBuffer() {
        for (int i = 0; i < 1; i++) {
            clientBuf[i].buffer = (uint8_t*)malloc(460800);
        }
        clientRevCount = 0;
        clientUsedCount = 0;
    }

    void reset() {
        for (int i = 0; i < 1; i++) {
            clientBuf[i].reset();
        }
        clientRevCount = clientUsedCount = 0;
        receivedFrameNo = decodedFrameNo = 0;
    }
};
extern std::mutex client_buf_mutex;
};  // namespace android

#endif  // HW_EMULATOR_CAMERA_VIRTUALD_CAMERA_FACTORY_H_K
