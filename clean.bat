pushd "%~dp0"

rd .\lib\memops_opt\test\__pycache__ /s /q
rd .\build /s /q

call .\lib\memops_opt\test\cleanresults.bat

popd