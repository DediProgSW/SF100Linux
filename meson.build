project('dpcmd', 'c',
    license : 'GPL-2.0',
    version : '1.14.21',
    default_options: {
        'optimization': 'O2',

    }
)

sources = files(
    'board.c',
    'dpcmd.c',
    'FlashCommand.c',
    'IntelHexFile.c',
    'MotorolaFile.c',
    'parse.c',
    'project.c',
    'SerialFlash.c',
    'usbdriver.c',
)

libusb = dependency('libusb-1.0', version : '>=1.0.0')
pthreads = dependency('threads')

executable('dpcmd',
    sources,
    dependencies : [libusb, pthreads],
)