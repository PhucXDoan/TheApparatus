@echo off
if not defined DevEnvDir (
	call vcvarsall.bat x64
)
