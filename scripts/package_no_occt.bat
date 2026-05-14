@echo off
powershell -ExecutionPolicy Bypass -File "%~dp0package_nuget.ps1" -Variant NoOcct %*
