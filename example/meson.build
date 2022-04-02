
examples_task = executable('task', 'task.cpp',
    include_directories:[base_inc_dir],
    dependencies : [thread,uring,atomic_dep],
    link_with : [
        scheduler_lib
    ]
)

examples_async = executable('async', 'async.cpp',
    include_directories:[base_inc_dir],
    dependencies : [thread,uring,atomic_dep],
    link_with : [
        scheduler_lib
    ]
)

examples_read_file = executable('read_file', 'read_file.cpp',
    include_directories:[base_inc_dir],
    dependencies : [thread,uring,atomic_dep],
    link_with : [
        scheduler_lib,
        io_lib
    ]
)

examples_http = executable('http', 'http.cpp',
    include_directories:[base_inc_dir],
    dependencies : [thread,uring,atomic_dep],
    link_with : [
        scheduler_lib,
        io_lib
    ]
)