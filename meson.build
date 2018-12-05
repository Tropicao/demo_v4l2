project('demo_v4l2', 'c')

jpeg_dep = dependency('libjpeg')

src = [
  'src/main.c',
  'src/yuv_fetcher.c',
  'src/jpeg_encoder.c',
]

executable('demo_v4l2',
  sources : src,
  dependencies : [jpeg_dep]
  )
