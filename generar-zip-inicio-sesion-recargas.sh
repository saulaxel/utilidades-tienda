#!/bin/bash
SCRIPT_PATH="$(cd "$(dirname "$0")" && pwd)"
zip -j -r $SCRIPT_PATH/Output/inicio-sesion-recargas.zip $SCRIPT_PATH/inicio-sesion-recargas/*
