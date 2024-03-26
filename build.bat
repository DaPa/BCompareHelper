:: MSBuild.exe should be found on path like C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64
echo off
"MSBuild.exe" BCompareHelper.sln /p:Platform="x64" /p:Configuration="Release"
pause
