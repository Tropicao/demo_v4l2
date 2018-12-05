#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>

#include "utils.h"
#include "yuv_fetcher.h"

static int _print_cap = 0;
static char _device[DEVICE_NAME_MAX_SIZE] = {0};
static char _output_dir[OUTPUT_DIR_NAME_MAX_SIZE] = {0};
static char _format[FORMAT_MAX_SIZE] = {0};
static int _print_help = 0;
static int _print_formats = 0;
static int _print_controls = 0;

static void _usage(char *progname)
{
    fprintf(stderr, "Usage : %s [-c] [-d device] [-o directory [-f format]]\n", progname);
    fprintf(stderr, "Options :\n");
    fprintf(stderr, "  -c           print video device capabilities and quit\n");
    fprintf(stderr, "  -C           print video controls capabilities and quit\n");
    fprintf(stderr, "  -d           device to use (default: /dev/video0)\n");
    fprintf(stderr, "  -f           output format (can be 'raw' or 'jpeg', default = raw)\n");
    fprintf(stderr, "  -F           print device formats and quit\n");
    fprintf(stderr, "  -h           prints this help\n");
    fprintf(stderr, "  -o           output directory to use (default: local directory)\n");
}

static void _int_handler(int sig)
{
    INF("Main loop interrupted");
    yuv_fetcher_stop();
};

static int _parse_args(int argc, char *argv[])
{
    int c = 0;
    while ((c = getopt (argc, argv, "cCd:f:Fho:")) != -1)
    {
        switch (c)
        {
            case 'c':
                _print_cap = 1;
                break;
            case 'C':
                _print_controls = 1;
                break;
            case 'd':
                strncpy(_device, optarg, DEVICE_NAME_MAX_SIZE);
                break;
            case 'f':
                strncpy(_format, optarg, FORMAT_MAX_SIZE);
                if(strncmp(_format, "raw", FORMAT_MAX_SIZE) != 0 &&
                   strncmp(_format, "jpeg", FORMAT_MAX_SIZE) != 0)
                {
                    _usage(argv[0]);
                    return 1;
                }
                break;
            case 'F':
                _print_formats = 1;
                break;
            case 'h':
                _print_help = 1;
                break;
            case 'o':
                strncpy(_output_dir, optarg, OUTPUT_DIR_NAME_MAX_SIZE);
                break;
            default:
                _usage(argv[0]);
                return 1;
                break;
        }
    }

    /* Prevent passing format if output directory has not been passed */
    if (_format[0] != 0 && _output_dir[0] == 0)
    {
        _usage(argv[0]);
        return 1;
    }

    return 0;
}


static void _start_main_loop()
{
    // Blocking call
    yuv_fetcher_start(_output_dir, _format);
}

int main(int argc, char *argv[])
{
    int run_capture = 0;
    if(_parse_args(argc, argv) != 0)
        return 1;

    if(_print_help)
    {
        _usage(argv[0]);
        return 0;
    }

    INF("**************************");
    INF("***      V4L2 demo     ***");
    INF("**************************\n");

    signal(SIGINT, _int_handler);

    /* If program has been started only to look at some capabilities, do not fully initialize capture */
    run_capture = !(
            _print_cap ||
            _print_formats ||
            _print_controls);

    if(yuv_fetcher_init(run_capture, _device) != 0)
    {
        ERR("Cannot initialize YUV fetcher");
        return 1;
    }
    
    if(_print_controls)
        yuv_fetcher_print_controls();
    if(_print_formats)
        yuv_fetcher_print_avail_formats();
    if(_print_cap)
        yuv_fetcher_print_capabilities();

    if(run_capture)
        _start_main_loop ();

    yuv_fetcher_shutdown();
    return 0;
}
