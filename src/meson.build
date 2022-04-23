smp_src = [
    'coroutine/timer.cpp',
    'coroutine/scheduler/scheduler.cpp',
    'io/io_service.cpp'
    ]

smp_lib = library('smb',smp_src,
    include_directories:[base_inc_dir],
    dependencies : [
        thread,
        uring,
        atomic_dep
    ],
    link_with : [
    ]
)