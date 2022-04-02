io_src = [
    'io_service.cpp'
    ]

io_lib = library('io',io_src,
    include_directories:[base_inc_dir],
    dependencies : [
        thread,
        uring,
        atomic_dep
    ],
    link_with : [
    ]
)