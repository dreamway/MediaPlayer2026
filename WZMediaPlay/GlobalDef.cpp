#include "GlobalDef.h"

GlobalDef GlobalDef::mGlobalDef;

GlobalDef::GlobalDef() {}

GlobalDef::~GlobalDef() {}

GlobalDef *GlobalDef::getInstance()
{
    return &mGlobalDef;
}

void GlobalDef::SetRunningMode(RunningMode mode)
{
    mRunningMode = mode;
}

extern spdlog::logger *logger = nullptr;