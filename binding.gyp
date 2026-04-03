{
  "targets": [
    {
      "target_name": "nimage",
      "sources": [
        "src/binding.cpp",
        "src/decoder.cpp"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "src",
        "<(module_root_dir)/deps/include"
      ],
      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")"
      ],
      "defines": [
        "NAPI_DISABLE_CPP_EXCEPTIONS"
      ],
      "cflags!": ["-fno-exceptions"],
      "cflags_cc!": ["-fno-exceptions"],
      "conditions": [
        ["OS=='win'", {
          "defines": [
            "WIN32_LEAN_AND_MEAN",
            "NOMINMAX",
            "__STDC_CONSTANT_MACROS"
          ],
          "msvs_settings": {
            "VCCLCompilerTool": {
              "ExceptionHandling": 1,
              "AdditionalOptions": ["/std:c++17", "/EHsc"]
            }
          },
          "configurations": {
            "Release": {
              "cflags": ["-O2"],
              "cflags_cc": ["-O2", "-std=c++17"]
            },
            "Debug": {
              "cflags": ["-g"],
              "cflags_cc": ["-g", "-std=c++17"]
            }
          },
          "libraries": [
            "-L<(module_root_dir)/deps/lib",
            "-lraw_r",
            "-lheif",
            "-luser32",
            "-lgdi32"
          ],
          "copies": [
            {
              "destination": "<(module_root_dir)/build/Release",
              "files": [
                "<(module_root_dir)/deps/bin/*.dll"
              ]
            }
          ]
        }],
        ["OS=='linux'", {
          "defines": [
            "__STDC_CONSTANT_MACROS"
          ],
          "cflags": ["-std=c++17"],
          "cflags_cc": ["-std=c++17"],
          "link_settings": {
            "library_dirs": [
              "<(module_root_dir)/deps/lib"
            ],
            "libraries": [
              "-lraw_r",
              "-lheif"
            ]
          }
        }]
      ]
    }
  ]
}
