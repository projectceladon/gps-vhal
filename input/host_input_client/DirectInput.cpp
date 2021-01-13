//#define CLOG_NDEBUG 0
#define CLOG_TAG "DirectInputReceiver"

#include "DirectInput.h"
//#include "Log.h"

DirectInputReceiver::DirectInputReceiver(int id, int inputId)
{
    if ((mDebug & 0x1) > 0)
        printf("%s\n", __func__);
    mInstanceId = id;
    mInputId = inputId;
    CreateTouchDevice(id, inputId);
}

DirectInputReceiver::~DirectInputReceiver()
{
    if ((mDebug & 0x1) > 0)
        printf("%s\n", __func__);

    if (mFd >= 0)
    {
        close(mFd);
    }
}

bool DirectInputReceiver::CreateTouchDevice(int id, int inputId)
{
    if ((mDebug & 0x1) > 0)
        printf("%s\n", __func__);

    // Todo: use socket to support multiple input for each display
    char path[256];
    const char *dir = getenv(kEnvWorkDir);

    if (access(kDevName, F_OK) == 0)
    {
        snprintf(path, 256, "%s", kDevName);
    }
    else if (dir && (strlen(dir) + strlen(kDevName) < 256))
    {
        snprintf(path, 256, "%s/%s", dir, kDevName);
    }
    else
    {
        snprintf(path, 256, "%s/%s%d-%d", "./workdir", kDevName, id, inputId);
    }

    mFd = open(path, O_RDWR | O_NONBLOCK, 0);
    if (mFd < 0)
    {
        fprintf(stderr, "Failed to open pipe for read:%s\n", strerror(errno));
        return false;
    }

    fprintf(stderr, "Open %s as pipe\n", path);
    return true;
}

bool DirectInputReceiver::SendEvent(uint16_t type,
                                    uint16_t code,
                                    int32_t value)
{
    struct input_event ev;
    timespec ts;

    if (mFd < 0)
    {
        CreateTouchDevice(mInstanceId, mInputId);
        if (mFd < 0)
            return false;
    }

    memset(&ev, 0, sizeof(struct input_event));
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ev.time.tv_sec = ts.tv_sec;
    ev.time.tv_usec = ts.tv_nsec / 1000;
    ev.type = type;
    ev.code = code;
    ev.value = value;

    if (write(mFd, &ev, sizeof(struct input_event)) < 0)
    {
        perror("Failed to send event\n");
        return false;
    }
    return true;
}

bool DirectInputReceiver::SendDown(int32_t slot,
                                   int32_t x,
                                   int32_t y,
                                   int32_t pressure)
{
    if ((mDebug & 0x1) > 0)
        printf("%s\n", __func__);

    if ((uint32_t)slot >= kMaxSlot)
    {
        return false;
    }
    if (mContacts[slot].enabled || mEnabledSlots >= kMaxSlot || slot == 8)
    {
        SendReset();
    }
    mContacts[slot].enabled = true;
    mContacts[slot].trackingId = mTrackingId++;
    mEnabledSlots++;

    SendEvent(EV_ABS, ABS_MT_SLOT, slot);
    SendEvent(EV_ABS, ABS_MT_TRACKING_ID, mContacts[slot].trackingId);

    if (mEnabledSlots == 1)
    {
        SendEvent(EV_KEY, BTN_TOUCH, 1);
    }
    SendEvent(EV_ABS, ABS_MT_TOUCH_MAJOR, 0x00000004);
    SendEvent(EV_ABS, ABS_MT_WIDTH_MAJOR, 0x00000006);
    SendEvent(EV_ABS, ABS_MT_PRESSURE, pressure);
    SendEvent(EV_ABS, ABS_MT_POSITION_X, x);
    SendEvent(EV_ABS, ABS_MT_POSITION_Y, y);

    return true;
}

bool DirectInputReceiver::SendUp(int32_t slot, int32_t x, int32_t y)
{
    if ((mDebug & 0x1) > 0)
        printf("%s\n", __func__);

    if (mEnabledSlots == 0 || (uint32_t)slot >= kMaxSlot ||
        !mContacts[slot].enabled)
    {
        return false;
    }

    mContacts[slot].enabled = false;
    mEnabledSlots--;

    SendEvent(EV_ABS, ABS_MT_SLOT, slot);
    SendEvent(EV_ABS, ABS_MT_TRACKING_ID, -1);
    if (mEnabledSlots == 0)
    {
        SendEvent(EV_KEY, BTN_TOUCH, 0);
    }
    return true;
}

bool DirectInputReceiver::SendMove(int32_t slot,
                                   int32_t x,
                                   int32_t y,
                                   int32_t pressure)
{
    if ((mDebug & 0x1) > 0)
        printf("%s\n", __func__);

    if ((uint32_t)slot >= kMaxSlot || !mContacts[slot].enabled)
    {
        return false;
    }

    SendEvent(EV_ABS, ABS_MT_SLOT, slot);
    SendEvent(EV_ABS, ABS_MT_TOUCH_MAJOR, 0x00000004);
    SendEvent(EV_ABS, ABS_MT_WIDTH_MAJOR, 0x00000006);
    SendEvent(EV_ABS, ABS_MT_PRESSURE, pressure);
    SendEvent(EV_ABS, ABS_MT_POSITION_X, x);
    SendEvent(EV_ABS, ABS_MT_POSITION_Y, y);

    return true;
}

bool DirectInputReceiver::SendCommit()
{
    if ((mDebug & 0x1) > 0)
        printf("%s\n", __func__);

    SendEvent(EV_SYN, SYN_REPORT, 0);
    return true;
}

bool DirectInputReceiver::SendReset()
{
    if ((mDebug & 0x1) > 0)
        printf("%s\n", __func__);

    bool report = false;
    for (uint32_t slot = 0; slot < kMaxSlot; slot++)
    {
        if (mContacts[slot].enabled)
        {
            mContacts[slot].enabled = false;
            report = true;
        }
    }
    if (report)
    {
        SendEvent(EV_SYN, SYN_REPORT, 0);
    }
    return true;
}

void DirectInputReceiver::SendWait(uint32_t ms)
{
    if ((mDebug & 0x1) > 0)
        printf("%s\n", __func__);

    usleep(ms * 1000);
}

bool DirectInputReceiver::ProcessOneCommand(const std::string &cmd)
{
    if ((mDebug & 0x1) > 0)
        printf("%s\n", __func__);

    char type = 0;
    int32_t slot = 0;
    int32_t x = 0;
    int32_t y = 0;
    int32_t pressure = 0;
    int32_t ms = 0;

    // CLOGD("%s:%d %s", __func__, __LINE__, cmd.c_str());

    switch (cmd[0])
    {
    case 'c': // commit
        if ((mDebug & 0x1) > 0)
            printf("SendCommit\n");
        SendCommit();
        break;
    case 'r': // reset
        SendReset();
        break;
    case 'd': // down
        sscanf(cmd.c_str(), "%c %d %d %d %d", &type, &slot, &x, &y, &pressure);
        if ((mDebug & 0x1) > 0)
            printf("SendDown slot %d, x %d, y %d, pressure %d\n", slot, x, y, pressure);
        SendDown(slot, x, y, pressure);
        break;
    case 'u': // up
        sscanf(cmd.c_str(), "%c %d %d %d", &type, &slot, &x, &y);
        if ((mDebug & 0x1) > 0)
            printf("SendUp slot %d\n", slot);
        SendUp(slot, x, y);
        break;
    case 'm': // move
        sscanf(cmd.c_str(), "%c %d %d %d %d", &type, &slot, &x, &y, &pressure);
        if ((mDebug & 0x1) > 0)
            printf("SendMove slot %d, x %d, y %d, pressure %d\n", slot, x, y, pressure);
        SendMove(slot, x, y, pressure);
        break;
    case 'w': // wait ms
        sscanf(cmd.c_str(), "%c %d", &type, &ms);
        SendWait(ms);
        break;
    default:
        break;
    }
    return true;
}

bool DirectInputReceiver::joystickEnable()
{
    if (mDebug)
        if ((mDebug & 0x1) > 0)
            printf("%s:%d enable joystick down\n", __func__, __LINE__);
    onJoystickMessage("k 631 1\n");
    onJoystickMessage("c\n");
    usleep(2000);
    if (mDebug)
        if ((mDebug & 0x1) > 0)
            printf("%s:%d enable joystick up\n", __func__, __LINE__);
    onJoystickMessage("k 631 0\n");
    onJoystickMessage("c\n");
    mJoystickStatus = true;
    return true;
}

bool DirectInputReceiver::joystickDisable()
{
    if (mDebug)
        if ((mDebug & 0x1) > 0)
            printf("%s:%d disable joystick down\n", __func__, __LINE__);
    onJoystickMessage("k 632 1\n");
    onJoystickMessage("c\n");
    usleep(2000);
    if (mDebug)
        if ((mDebug & 0x1) > 0)
            printf("%s:%d disable joystick up\n", __func__, __LINE__);
    onJoystickMessage("k 632 0\n");
    onJoystickMessage("c\n");
    mJoystickStatus = false;
    return true;
}

bool DirectInputReceiver::getJoystickStatus()
{
    return mJoystickStatus;
}

bool DirectInputReceiver::ProcessOneJoystickCommand(const std::string &cmd)
{
    if ((mDebug & 0x1) > 0)
        printf("%s\n", __func__);

    char type = 0;
    uint16_t code;
    int32_t value;

    switch (cmd[0])
    {
    case 'c': // Commit
        if ((mDebug & 0x1) > 0)
            printf("SendCommit\n");
        SendCommit();
        break;

    case 'k': // EV_KEY 1
        sscanf(cmd.c_str(), "%c %" SCNu16 " %" SCNd32, &type, &code, &value);
        if ((mDebug & 0x1) > 0)
            printf("code = %d, value = %d\n", code, value);
        SendEvent(EV_KEY, code, value);
        break;

    case 'm': // EV_MSC 4
        sscanf(cmd.c_str(), "%c %" SCNu16 " %" SCNd32, &type, &code, &value);
        if ((mDebug & 0x1) > 0)
            printf("code = %d, value = %d\n", code, value);
        SendEvent(EV_MSC, code, value);
        break;

    case 'a': // EV_ABS 3
        sscanf(cmd.c_str(), "%c %" SCNu16 " %" SCNd32, &type, &code, &value);
        if ((mDebug & 0x1) > 0)
            printf("code = %d, value = %d\n", code, value);
        SendEvent(EV_ABS, code, value);
        break;
    default:
        break;
    }
    return true;
}

int DirectInputReceiver::getTouchInfo(TouchInfo *info)
{
    if ((mDebug & 0x1) > 0)
        printf("%s\n", __func__);

    if (!info)
    {
        return -EINVAL;
    }

    info->max_contacts = kMaxSlot;
    info->max_pressure = kMaxPressure;
    info->max_x = kMaxPositionX;
    info->max_y = kMaxPositionY;
    info->pid = getpid();
    info->version = 1;

    return 0;
}
int DirectInputReceiver::onInputMessage(const std::string &msg)
{
    if ((mDebug & 0x1) > 0)
        printf("%s\n", __func__);

    size_t begin = 0;
    size_t end = 0;

    // CLOGD("%s:%d %s", __func__, __LINE__, msg.c_str());

    while (true)
    {
        end = msg.find("\n", begin);
        if (end == std::string::npos)
            break;

        std::string cmd = msg.substr(begin, end);
        ProcessOneCommand(cmd);
        begin = end + 1;
        if (msg[begin] == '\r')
            begin++;
    }
    return 0;
}

int DirectInputReceiver::onKeyCode(uint16_t scanCode, uint32_t mask)
{
    if ((mDebug & 0x1) > 0)
        printf("%s\n", __func__);

    if (mask & KEY_STATE_MASK::Shift)
    {
        SendEvent(EV_KEY, KEY_LEFTSHIFT, 1);
        SendCommit();
    }
    if (mask & KEY_STATE_MASK::Control)
    {
        SendEvent(EV_KEY, KEY_LEFTCTRL, 1);
        SendCommit();
    }
    if (mask & KEY_STATE_MASK::Mod1)
    {
        SendEvent(EV_KEY, KEY_LEFTALT, 1);
        SendCommit();
    }

    SendEvent(EV_KEY, scanCode, 1);
    SendCommit();
    SendEvent(EV_KEY, scanCode, 0);
    SendCommit();

    if (mask & KEY_STATE_MASK::Shift)
    {
        SendEvent(EV_KEY, KEY_LEFTSHIFT, 0);
        SendCommit();
    }
    if (mask & KEY_STATE_MASK::Control)
    {
        SendEvent(EV_KEY, KEY_LEFTCTRL, 0);
        SendCommit();
    }
    if (mask & KEY_STATE_MASK::Mod1)
    {
        SendEvent(EV_KEY, KEY_LEFTALT, 0);
        SendCommit();
    }
    return 0;
}

int DirectInputReceiver::onJoystickMessage(const std::string &msg)
{
    if ((mDebug & 0x1) > 0)
        printf("%s\n", __func__);

    size_t begin = 0;
    size_t end = 0;

    // CLOGD("%s:%d %s", __func__, __LINE__, msg.c_str());

    while (true)
    {
        end = msg.find("\n", begin);
        if (end == std::string::npos)
            break;

        std::string cmd = msg.substr(begin, end);
        ProcessOneJoystickCommand(cmd);
        begin = end + 1;
        if (msg[begin] == '\r')
            begin++;
    }
    return 0;
}

int DirectInputReceiver::onKeyChar(char ch)
{
    if ((mDebug & 0x1) > 0)
        printf("%s\n", __func__);

    return 0;
}

int DirectInputReceiver::onText(const char *msg)
{
    if ((mDebug & 0x1) > 0)
        printf("%s\n", __func__);

    return 0;
}
