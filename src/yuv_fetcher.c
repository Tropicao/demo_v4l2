#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>
#include <sys/mman.h>

#include "yuv_fetcher.h"
#include "jpeg_encoder.h"
#include "utils.h"

#define FILE_NAME_MAX_SIZE      128

typedef struct
{
    void *start;
    size_t length;
} nv21_buffer;

static int _webcam_fd = -1;
static int _error = 0;
static nv21_buffer *buffers = NULL;
static uint8_t  loop_run = 0;
static yuv_data_callback_t _data_cb = NULL;
static char _output_dir[OUTPUT_DIR_NAME_MAX_SIZE] = {0};
static char _format[FORMAT_MAX_SIZE] = {0};

static int xioctl(int fh, int request, void *arg)
{
    int r;

    do {
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);

    return r;
}

static int _open_device(char *device)
{
    int error = 0;
    if(device[0] != 0)
    {
        INF("Opening device %s", device);
        _webcam_fd = open(device, O_RDWR);
    }
    else
    {
        INF("Opening device %s", DEVICE_NAME_DEFAULT);
        _webcam_fd = open(DEVICE_NAME_DEFAULT, O_RDWR);
    }

    if(_webcam_fd < 0)
    {
        ERR("Cannot open video device");
        error = -1;
    }
    return error;
}

static void _enumerate_menu(struct v4l2_queryctrl *queryctrl)
{
    struct v4l2_querymenu querymenu;

    memset(&querymenu, 0, sizeof(querymenu));
    querymenu.id = queryctrl->id;

    for (querymenu.index = (__u32)queryctrl->minimum;
         querymenu.index <= (__u32)queryctrl->maximum;
         querymenu.index++)
    {
        if (0 == xioctl(_webcam_fd, VIDIOC_QUERYMENU, &querymenu))
        {
            INF("\t* %s", querymenu.name);
        }
    }
}

static int _enumerate_controls(void)
{
    struct v4l2_queryctrl queryctrl;

    memset(&queryctrl, 0, sizeof(queryctrl));

    INF("Starting controls enumeration");
    queryctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
    while (0 == xioctl(_webcam_fd, VIDIOC_QUERYCTRL, &queryctrl))
    {
        if ((queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) == 0)
        {
            INF("Found control : %s", queryctrl.name);

            if (queryctrl.type == V4L2_CTRL_TYPE_MENU)
                _enumerate_menu(&queryctrl);
        }

        queryctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
    }
    INF("End of controls enumeration");

    if (errno != EINVAL)
    {
        ERR("Error while enumerating controls : %s", strerror(errno));
        return -1;
    }

    return 0;
}

static void _print_capabilities(__u32 cap)
{
    if(cap & V4L2_CAP_VIDEO_CAPTURE)
        INF("V4L2_CAP_VIDEO_CAPTURE");
    if(cap & V4L2_CAP_VIDEO_OUTPUT)
        INF("V4L2_CAP_VIDEO_OUTPUT");
    if(cap & V4L2_CAP_VIDEO_OVERLAY)
        INF("V4L2_CAP_VIDEO_OVERLAY");
    if(cap & V4L2_CAP_VBI_CAPTURE)
        INF("V4L2_CAP_VBI_CAPTURE");
    if(cap & V4L2_CAP_VBI_OUTPUT)
        INF("V4L2_CAP_VBI_OUTPUT");
    if(cap & V4L2_CAP_SLICED_VBI_CAPTURE)
        INF("V4L2_CAP_SLICED_VBI_CAPTURE");
    if(cap & V4L2_CAP_SLICED_VBI_OUTPUT)
        INF("V4L2_CAP_SLICED_VBI_OUTPUT");
    if(cap & V4L2_CAP_RDS_CAPTURE)
        INF("V4L2_CAP_RDS_CAPTURE");
    if(cap & V4L2_CAP_VIDEO_OUTPUT_OVERLAY)
        INF("V4L2_CAP_VIDEO_OUTPUT_OVERLAY");
    if(cap & V4L2_CAP_HW_FREQ_SEEK)
        INF("V4L2_CAP_HW_FREQ_SEEK");
    if(cap & V4L2_CAP_RDS_OUTPUT)
        INF("V4L2_CAP_RDS_OUTPUT");
    if(cap & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
        INF("V4L2_CAP_VIDEO_CAPTURE_MPLANE");
    if(cap & V4L2_CAP_VIDEO_OUTPUT_MPLANE)
        INF("V4L2_CAP_VIDEO_OUTPUT_MPLANE");
    if(cap & V4L2_CAP_TUNER)
        INF("V4L2_CAP_TUNER");
    if(cap & V4L2_CAP_AUDIO)
        INF("V4L2_CAP_AUDIO");
    if(cap & V4L2_CAP_RADIO)
        INF("V4L2_CAP_RADIO");
    if(cap & V4L2_CAP_MODULATOR)
        INF("V4L2_CAP_MODULATOR");
    if(cap & V4L2_CAP_READWRITE)
        INF("V4L2_CAP_READWRITE");
    if(cap & V4L2_CAP_ASYNCIO)
        INF("V4L2_CAP_ASYNCIO");
    if(cap & V4L2_CAP_STREAMING)
        INF("V4L2_CAP_STREAMING");
    if(cap & V4L2_CAP_DEVICE_CAPS)
        INF("V4L2_CAP_DEVICE_CAPS");
}

static const char *_get_fmt_field_string(__u32 field)
{
    switch(field)
    {
        case V4L2_FIELD_ANY:
            return "ANY";
            break;
        case V4L2_FIELD_NONE:
            return "NONE (PROGRESSIVE)";
            break;
        case V4L2_FIELD_TOP:
            return "TOP";
            break;
        case V4L2_FIELD_BOTTOM:
            return "BOTTOM";
            break;
        case V4L2_FIELD_INTERLACED:
            return "INTERLACED";
            break;
        case V4L2_FIELD_SEQ_TB:
            return "SEQUENCIAL : TOP->BOTTOM";
            break;
        case V4L2_FIELD_SEQ_BT:
            return "SEQUENTIAL : BOTTOM->TOP";
            break;
        case V4L2_FIELD_ALTERNATE:
            return "ALTERNATE";
            break;
        case V4L2_FIELD_INTERLACED_TB:
            return "INTERLACED TOP->BOTTOM";
            break;
        case V4L2_FIELD_INTERLACED_BT:
            return "INTERLACED BOTTOM->TOP";
            break;
        default:
            return "UNKNOWN";
            break;
    }
}

static const char* _get_fmt_colorspace_string(__u32 colorspace)
{
    switch(colorspace)
    {
        case V4L2_COLORSPACE_SMPTE170M:
            return "SMPTE170M";
            break;
        case V4L2_COLORSPACE_SMPTE240M:
            return "SMPTE240M";
            break;
        case V4L2_COLORSPACE_REC709:
            return "REC709";
            break;
        case V4L2_COLORSPACE_BT878:
            return "BT878";
            break;
        case V4L2_COLORSPACE_470_SYSTEM_M :
            return "470 SYSTEM M";
            break;
        case V4L2_COLORSPACE_470_SYSTEM_BG:
            return "470 SYSTEM BG";
            break;
        case V4L2_COLORSPACE_JPEG:
            return "JPEG";
            break;
        case V4L2_COLORSPACE_SRGB:
            return "SRBG";
            break;
        default:
            return "UNKNOWN";
            break;
    }
}

static void _print_format_parameters(struct v4l2_format format)
{
    INF("> Width        : %d", format.fmt.pix.width);
    INF("> Height       : %d", format.fmt.pix.height);
    INF("> Format       : %c%c%c%c",
             format.fmt.pix.pixelformat & 0xFF,
            (format.fmt.pix.pixelformat >> 8) & 0xFF,
            (format.fmt.pix.pixelformat >> 16) & 0xFF,
            (format.fmt.pix.pixelformat >> 24) & 0xFF);
    INF("> Field        : %s", _get_fmt_field_string(format.fmt.pix.field));
    INF("> Bytes/line   : %d", format.fmt.pix.bytesperline);
    INF("> Image size   : %d", format.fmt.pix.sizeimage);
    INF("> Color space  : %s", _get_fmt_colorspace_string(format.fmt.pix.colorspace));
}

static void _print_general_info(void)
{
    struct v4l2_capability cap;
    /* Ask for V4L2 device capabilities */
    if(xioctl(_webcam_fd, VIDIOC_QUERYCAP, &cap) == -1)
    {
        ERR("Cannot query video capture device capabilities : %s", strerror(errno));
        return;
    }
    else
    {
        INF("Successfully retrieved video capture device capabilities");
        INF("\t* driver         : %s", cap.driver);
        INF("\t* card           : %s", cap.card);
        INF("\t* buf_info       : %s", cap.bus_info);
        INF("\t* version        : %u.%u.%u",
                (cap.version >> 16) & 0xFF,
                (cap.version >> 8) & 0xFF,
                (cap.version & 0xFF));
        INF("\t* capabilites    : 0x%08X\n", cap.capabilities);
    }
}


static int _setup_video_cap(void)
{
    struct v4l2_input input;
    struct v4l2_format format;
    struct v4l2_streamparm params;

    int error = 0;

    /* Set video input (0 by default for the demo */
    memset(&input, 0, sizeof(input));
    input.index=0;
    if(xioctl(_webcam_fd, VIDIOC_S_INPUT, &input) == -1)
    {
        ERR("Cannot set current video input data : %s", strerror(errno));
        error = -1;
        goto setup_end;
    }
    /* Get framerate */
    params.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(xioctl(_webcam_fd, VIDIOC_G_PARM, &params) == -1)
    {
        ERR("Cannot get capture input parameters : %s", strerror(errno));
        error = -1;
        goto setup_end;
    }
    INF("Current framerate : %d/%d", params.parm.capture.timeperframe.numerator, params.parm.capture.timeperframe.denominator);
    params.parm.capture.timeperframe.numerator = 1;
    params.parm.capture.timeperframe.numerator = FRAME_RATE;
    if(xioctl(_webcam_fd, VIDIOC_S_PARM, &params) == -1)
    {
        ERR("Cannot set capture input parameters : %s", strerror(errno));
        error = -1;
        goto setup_end;
    }
    /* Select YUV format */
    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(xioctl(_webcam_fd, VIDIOC_G_FMT, &format) == -1)
    {
        ERR("Cannot get current format parameters : %s", strerror(errno));
        error = -1;
        goto setup_end;
    }

    INF("Setting image format to %dx%d", FRAME_WIDTH, FRAME_HEIGHT);
    format.fmt.pix.width = FRAME_WIDTH;
    format.fmt.pix.height = FRAME_HEIGHT;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_NV21;
    format.fmt.pix.field = V4L2_FIELD_ANY;
    format.fmt.pix.bytesperline = FRAME_WIDTH;
    if(xioctl(_webcam_fd, VIDIOC_S_FMT, &format) == -1)
    {
        ERR("Error while setting YUV format : %s", strerror(errno));
        error = -1;
        goto setup_end;
    }

    _print_format_parameters(format);

    INF("Allocating frame buffer");

setup_end:
    return error;
}

static int _configure_buffers(void)
{
    struct v4l2_requestbuffers reqbuf;
    struct v4l2_buffer buffer;
    __u32 i = 0;
    int error = 0;

    INF("Configuring buffers");
    memset(&reqbuf, 0, sizeof(reqbuf));
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    reqbuf.count = NB_BUF;

    if(xioctl(_webcam_fd, VIDIOC_REQBUFS, &reqbuf) == -1)
    {
        ERR("Cannot request buffers to driver : %s", strerror(errno));
        return -1;
    }

    if(reqbuf.count != NB_BUF)
    {
        ERR("Driver did not give as many buffer as requested (wanted : %d, got %d)",
                NB_BUF, reqbuf.count);
        return -1;
    }

    buffers = calloc(reqbuf.count, sizeof(nv21_buffer));
    if(!buffers)
    {
        ERR("Error allocating buffer structures");
        error = 1;
        goto buf_end;
    }

    INF("Starting configuration of %d buffers", reqbuf.count);
    for (i=0; i<reqbuf.count; i++)
    {
        memset(&buffer, 0, sizeof(buffer));
        buffer.type = reqbuf.type;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;

        INF("Configuring buffer %d", i);
        if(xioctl(_webcam_fd, VIDIOC_QUERYBUF, &buffer) < 0)
        {
            ERR("Cannot claim buffer %d", i);
            error = -1;
            goto buf_end;
        }
        buffers[i].length = buffer.length;
        buffers[i].start = mmap(NULL, buffer.length,
                PROT_READ | PROT_WRITE,
                MAP_SHARED,
                _webcam_fd, buffer.m.offset);
        if(buffers[i].start == MAP_FAILED)
        {
            ERR("Failed to mmap buffer %d : %s", i, strerror(errno));
            error = -1;
            goto buf_end;
        }
        INF("Buffer %d mapped to %p - size %d", i, (void *)buffers[i].start, FRAME_SIZE);

    }

buf_end:
        return error;
}

static void _free_buffers(void)
{
    int i = 0;
    if(buffers)
    {
       for(i = 0; i < NB_BUF; i++)
       {
           munmap(buffers[i].start, buffers[i].length);
       }

    }
}

static void _dump_frame(void *data)
{
    static int frame_num;
    int fd = -1;
    char file_name[FILE_NAME_MAX_SIZE] = {0};
    int ret = 0;
    uint8_t *dest_buf = NULL;
    unsigned long frame_size = 0;

    if(!data)
    {
        ERR("Cannot dump empty data");
        return;
    }

    snprintf(file_name, FILE_NAME_MAX_SIZE, "%s/frame_%d.%s", _output_dir, frame_num, _format);
    fd = open(file_name, O_WRONLY|O_CREAT, S_IWUSR|S_IRUSR);
    if(fd < 0)
    {
        ERR("Cannot open file %s to dump frame %d", file_name, frame_num);
    }

    if(strncmp(_format, "jpeg", FORMAT_MAX_SIZE) == 0)
    {
        dest_buf = jpeg_encoder_encode_frame((uint8_t *)data, &frame_size);
        if(!dest_buf)
        {
            ERR("Error encountered while encoding jpeg, abort frame dump");
            return;
        }
    }
    else // Dealing with RAW image
    {
        dest_buf = data;
        frame_size = FRAME_SIZE;
    }

    ret = write(fd, dest_buf, frame_size);
    if(ret < 0)
    {
        ERR("Did not manage to write data to file %s : %s", file_name, strerror(errno));
    }
    else if(ret != frame_size)
    {
        INF("Did not manage to dump complete frame : dumped %d/%ld bytes", ret, frame_size);
    }
    else
    {
        INF("Frame %s has been dumped", file_name);
    }
    close(fd);

    if(++frame_num >= NB_DUMP_FRAME)
        frame_num = 0;
    if(strncmp(_format, "jpeg", FORMAT_MAX_SIZE) == 0 && dest_buf)
        free(dest_buf);
}

static int _start_capture_loop()
{
    struct v4l2_buffer buffer;
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int index;
    int ret = 0;

    INF("Enqueuing all buffers");
    for(index = 0; index < NB_BUF; index++)
    {
        memset(&buffer, 0, sizeof(buffer));
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = index;
        if(xioctl(_webcam_fd, VIDIOC_QBUF, &buffer) == -1)
        {
            ERR("Cannot enqueue buffer %d : %s", index, strerror(errno));
        }
        INF("Buffer %d enqueued for capture", index);
    }

    if(xioctl(_webcam_fd, VIDIOC_STREAMON, &type) == -1)
    {
        ERR("Cannot start capture : %s", strerror(errno));
        return -1;
    }

    while(loop_run)
    {
        memset(&buffer, 0, sizeof(struct v4l2_buffer));
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        ret = xioctl(_webcam_fd, VIDIOC_DQBUF, &buffer);
        if(ret == -1 && errno == EINTR)
        {
            if(xioctl(_webcam_fd, VIDIOC_QBUF, &buffer) == -1)
            {
                ERR("Did not manage to put buffer %d back in queue : %s",
                        buffer.index, strerror(errno));
                return -1;
            }
            continue;
        }
        else if(ret != 0)
        {
            ERR("Did not manage to retrieve frame : %s", strerror(errno));
        }
        else
        {
            DBG("Fetched full frame from buffer %d - %d bytes", buffer.index, FRAME_SIZE);
        }

        /* Call YUV consumer callback */
        if(_data_cb)
            _data_cb(buffers[buffer.index].start);

        /* If output directory has been provided, dump data */
        if(_output_dir[0] != 0)
        {
            _dump_frame(buffers[buffer.index].start);
        }


        if(xioctl(_webcam_fd, VIDIOC_QBUF, &buffer) == -1)
        {
            ERR("Did not manage to put buffer %d back in queue : %s",
                    buffer.index, strerror(errno));
            return -1;
        }
    }

    if(xioctl(_webcam_fd, VIDIOC_STREAMOFF, &type) == -1)
    {
        ERR("Cannot stop capture : %s", strerror(errno));
        return -1;
    }
    INF("Capture stopped");

    return 0;
}

int yuv_fetcher_init(int full_init, char *device)
{
    /* Open device */
    if(_open_device(device) == -1)
    {
        _error = -1;
        goto end;
    }

    _print_general_info();

    if(!full_init)
        return 0;

    /* Video capture setup */
    if(_setup_video_cap() == -1)
    {
        _error = -1;
        goto end;
    }

    // If init has been called only to print capabilities, stop there
    if(_configure_buffers() == -1)
    {
        _error = -1;
        goto end;
    }

    return 0;
end:
    _free_buffers();
    if(_webcam_fd >= 0)
        close(_webcam_fd);
    return _error;
}

int yuv_fetcher_start(char * output_dir, char *format)
{
    loop_run = 1;
    strncpy(_output_dir, output_dir, OUTPUT_DIR_NAME_MAX_SIZE);
    strncpy(_format, format, FORMAT_MAX_SIZE);

    if(_output_dir[0] != 0 && access(basename(_output_dir), W_OK) != 0)
    {
        ERR("Cannot start capture : output directory %s invalid", _output_dir);
        return 1;
    }

    if(_start_capture_loop() == -1)
    {
        return 1;
    }
    return 0;
}

void yuv_fetcher_stop(void)
{
    loop_run = 0; /* Will interrupt the QBUF/DQBUF loop */
}

void yuv_fetcher_shutdown(void)
{
    INF("Closing capture device");
    close(_webcam_fd);
    _webcam_fd = -1;
}

void yuv_fetcher_register_data_callback(yuv_data_callback_t cb)
{
    _data_cb = cb;
}

void yuv_fetcher_print_avail_formats()
{
    struct v4l2_fmtdesc fmt_desc;

    if(_webcam_fd != -1)
    {
        memset(&fmt_desc, 0, sizeof(fmt_desc));
        fmt_desc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        while(xioctl(_webcam_fd, VIDIOC_ENUM_FMT, &fmt_desc) == 0)
        {
            INF("Supported format : %s (code %c%c%c%c)", fmt_desc.description,
                    fmt_desc.pixelformat & 0xFF,
                    (fmt_desc.pixelformat >> 8) & 0xFF,
                    (fmt_desc.pixelformat >> 16) & 0xFF,
                    (fmt_desc.pixelformat >> 24) & 0xFF);
            fmt_desc.index++;
        }

        if(errno != EINVAL)
        {
            ERR("Error while enumerating formats : %s", strerror(errno));
        }
    }
}

void yuv_fetcher_print_controls()
{
    /* List all available controls */
    if(_enumerate_controls() != 0)
    {
        ERR("Cannot display all available controls");
    }
}

void yuv_fetcher_print_capabilities(void)
{
    struct v4l2_capability cap;
    if(xioctl(_webcam_fd, VIDIOC_QUERYCAP, &cap) == -1)
    {
        ERR("Cannot query video capture device capabilities : %s", strerror(errno));
        return;
    }

    _print_capabilities(cap.capabilities);
}
