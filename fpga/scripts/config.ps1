<#
FPGA命令行工具的唯一共享配置源。

build.ps1和program.ps1都读取这个文件。以后更换开发板、器件、目标频率、
构建目录或默认固件时，只需要修改这里，不要在两个脚本中分别维护。
#>
$FpgaConfig = @{
    DefaultFirmware = "examples\blink\build\firmware.hex"
    DefaultOssCadSuite = "E:\oss-cad-suite"

    BuildRelativePath = "build\pic10f200"
    BitstreamName = "pic10f200.fs"

    OpenFpgaLoaderBoard = "tangnano1k"
    NextPnrDevice = "GW1NZ-LV1QN48C6/I5"
    GowinPackDevice = "GW1NZ-1"
    TargetFrequencyMHz = 27
}
