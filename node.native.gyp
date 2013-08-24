{
  'targets' : [
    #server
    { 
      'target_name' : 'webserver',
      'type' : 'executable',
      'sources' : [
        'webserver.cpp',
        'native',
        'http-parser/http_parser.c',
        'http-parser/http_parser.h',
        
      ],
      'include_dirs' : [
        'libuv/include',
        '../node.native'
      ],
      'libraries' : [
        'libuv/libuv.a'
      ],
      'conditions' : [
        ['OS=="mac"', {

          'xcode_settings': {
            'OTHER_CPLUSPLUSFLAGS' : ['-std=c++11','-stdlib=libc++'],
            'OTHER_LDFLAGS': ['-stdlib=libc++'],
            'ARCHS': '$(ARCHS_STANDARD_64_BIT)'
          },

          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/CoreServices.framework',
              '$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework'
            ],
          },

        }]
      ]
    },
    #client  
    { 
      'target_name' : 'webclient',
      'type' : 'executable',
      'sources' : [
        'webclient.cpp',
        'native',
        'http-parser/http_parser.c',
        'http-parser/http_parser.h',
        
      ],
      'include_dirs' : [
        'libuv/include',
        '../node.native'
      ],
      'libraries' : [
        'libuv/libuv.a'
      ],
      'conditions' : [
        ['OS=="mac"', {

          'xcode_settings': {
            'OTHER_CPLUSPLUSFLAGS' : ['-std=c++11','-stdlib=libc++'],
            'OTHER_LDFLAGS': ['-stdlib=libc++'],
            'ARCHS': '$(ARCHS_STANDARD_64_BIT)'
          },

          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/CoreServices.framework',
              '$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework'
            ],
          },

        }]
      ]
    }
  ]
}