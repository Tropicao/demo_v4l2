#ifndef YUV_FETCHER_H
#define YUV_FETCHER_H

typedef void (*yuv_data_callback_t)(void *);

int yuv_fetcher_init(int print_cap, char *device);
int yuv_fetcher_start(char * output_dir, char *format);
void yuv_fetcher_stop(void);
void yuv_fetcher_shutdown(void);
void yuv_fetcher_register_data_callback(yuv_data_callback_t cb);
void yuv_fetcher_print_avail_formats(void);
void yuv_fetcher_print_controls(void);
void yuv_fetcher_print_capabilities(void);

#endif
