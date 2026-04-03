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
        "<(module_root_dir)/deps/libraw",
        "<(module_root_dir)/deps/libheif/include"
      ],
      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")"
      ],
      "defines": [
        "NAPI_DISABLE_CPP_EXCEPTIONS",
        "UNICODE",
        "_UNICODE"
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
              "AdditionalOptions": ["/std:c++17"]
            },
            "VCLinkerTool": {
              "AdditionalLibraryDirectories": [
                "<(module_root_dir)/deps/win/lib"
              ]
            }
          },
          "msvs_toolset": "v143",
          "libraries": [
            "-luser32",
            "-lgdi32",
            "-lpthread"
          ],
          "copies": [
            {
              "destination": "<(module_root_dir)/build/Release",
              "files": []
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
              "<(module_root_dir)/deps/linux/lib"
            ],
            "libraries": [
              "-lpthread"
            ]
          }
        }]
      ]
    }
  ]
}
