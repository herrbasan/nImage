{
  "variables": {
    "vcpkg_root": "C:/vcpkg"
  },
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
        "<(vcpkg_root)/installed/x64-windows/include"
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
              "AdditionalOptions": ["/EHsc", "/std:c++17"]
            },
            "VCLinkerTool": {
              "DelayLoadDLLs": ["node.exe"],
              "AdditionalLibraryDirectories": [
                "<(vcpkg_root)/installed/x64-windows/lib"
              ]
            }
          },
          "configurations": {
            "Release": {
              "cflags": ["-O2"],
              "cflags_cc": ["-O2", "-std:c++17"],
              "msvs_settings": {
                "VCCLCompilerTool": {
                  "Optimization": 2,
                  "AdditionalOptions": ["/EHsc", "/std:c++17"]
                }
              }
            },
            "Debug": {
              "cflags": ["-Od", "-Zi"],
              "cflags_cc": ["-Od", "-Zi", "-std:c++17"],
              "msvs_settings": {
                "VCCLCompilerTool": {
                  "Optimization": 0,
                  "DebugInformationFormat": 3,
                  "AdditionalOptions": ["/EHsc", "/std:c++17"]
                }
              }
            }
          },
          "libraries": [
            "-l<(vcpkg_root)/installed/x64-windows/lib/raw_r.lib",
            "-l<(vcpkg_root)/installed/x64-windows/lib/heif.lib",
            "-luser32",
            "-lgdi32",
            "-lws2_32"
          ],
          "copies": [
            {
              "destination": "<(module_root_dir)/build/Release",
              "files": [
                "<(vcpkg_root)/installed/x64-windows/bin/heif.dll",
                "<(vcpkg_root)/installed/x64-windows/bin/raw_r.dll",
                "<(vcpkg_root)/installed/x64-windows/bin/raw.dll",
                "<(vcpkg_root)/installed/x64-windows/bin/libde265.dll",
                "<(vcpkg_root)/installed/x64-windows/bin/libx265.dll",
                "<(vcpkg_root)/installed/x64-windows/bin/zlib1.dll"
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
