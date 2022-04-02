scheduler_src = [
    'scheduler.cpp'
    ]

scheduler_lib = library('scheduler',scheduler_src,
    include_directories:[base_inc_dir],
    dependencies : [
        thread,
        uring,
        atomic_dep
    ],
    link_with : [
    ]
)