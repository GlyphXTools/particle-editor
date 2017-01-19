#include "Rescale.h"
#include "resource.h"
#include "UI/UI.h"
#include <cmath>
using namespace std;

struct RESCALE_OPTIONS
{
    float timeScale;
    float sizeScale;
};

static BOOL CALLBACK RescaleDialogFunc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	RESCALE_OPTIONS* options = (RESCALE_OPTIONS*)(LONG_PTR)GetWindowLongPtr(hWnd, GWL_USERDATA);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
		options = (RESCALE_OPTIONS*)lParam;
		SetWindowLong(hWnd, GWL_USERDATA, (LONG)(LONG_PTR)options);
        SPINNER_INFO si;
        si.IsFloat = true;
        si.Mask       = SPIF_VALUE | SPIF_RANGE;
        si.f.MinValue = 0.01f;
        si.f.MaxValue = FLT_MAX;

        si.f.Value = options->timeScale * 100; Spinner_SetInfo(GetDlgItem(hWnd, IDC_SPINNER1), &si);
        si.f.Value = options->sizeScale * 100; Spinner_SetInfo(GetDlgItem(hWnd, IDC_SPINNER2), &si);
        break;
    }

    case WM_COMMAND:
        if (lParam != 0)
        {
            UINT code = HIWORD(wParam);
            UINT id   = LOWORD(wParam);
            switch (code)
            {
            case BN_CLICKED:
                if (id == IDOK || id == IDCANCEL)
                {
                    SPINNER_INFO si;
                    si.Mask = SPIF_VALUE;
                    Spinner_GetInfo(GetDlgItem(hWnd, IDC_SPINNER1), &si); options->timeScale = si.f.Value / 100;
                    Spinner_GetInfo(GetDlgItem(hWnd, IDC_SPINNER2), &si); options->sizeScale = si.f.Value / 100;
                    EndDialog(hWnd, (id == IDOK));
                }
                break;
            }
        }
        break;
    }
    return FALSE;
}

static void DoRescaleGroup(ParticleSystem::Emitter::Group& group, float scale)
{
    group.cylinderHeight *= scale;
    group.cylinderRadius *= scale;
    group.valX *= scale; group.valY *= scale; group.valZ *= scale;
    group.minX *= scale; group.minY *= scale; group.minZ *= scale;
    group.maxX *= scale; group.maxY *= scale; group.maxZ *= scale;
    group.sideLength   *= scale;
    group.sphereRadius *= scale;
}

void DoRescaleEmitter(ParticleSystem::Emitter* emitter, float timeScale, float sizeScale)
{
    if (timeScale != 1.0f)
    {
        emitter->skipTime     *= timeScale;
        emitter->freezeTime   *= timeScale;
        emitter->initialDelay *= timeScale;
        emitter->lifetime     *= timeScale;
        if (!emitter->isWeatherParticle)
        {
            if (emitter->useBursts)
            {
                emitter->burstDelay *= timeScale;
                float n = (int)(1.0f / emitter->burstDelay * 10000.0f + 0.5f) / 10000.0f;
                if ((int)n == n && emitter->nParticlesPerBurst == 1 && emitter->nBursts == 0)
                {
                    // We can switch to Particles/Second type
                    emitter->useBursts = false;
                    emitter->nParticlesPerSecond = (int)n;
                }
            }
            else
            {
                float n = emitter->nParticlesPerSecond / timeScale;
                if ((int)n != n)
                {
                    // We can't express the scale by staying in the Particle/Second type,
                    // switch to infinite burst mode
                    emitter->useBursts          = true;
                    emitter->nBursts            = 0;
                    emitter->burstDelay         = 1.0f / n;
                    emitter->nParticlesPerBurst = 1;
                }
                else
                {
                    emitter->nParticlesPerSecond = (int)n;
                }
            }

            // Rescale speed and acceleration
            DoRescaleGroup(emitter->groups[ParticleSystem::GROUP_SPEED], 1 / timeScale);
            emitter->inwardSpeed        /= timeScale;
            emitter->inwardAcceleration /= timeScale;
            emitter->acceleration[0]    /= timeScale;
            emitter->acceleration[1]    /= timeScale;
            emitter->acceleration[2]    /= timeScale;
            emitter->gravity            /= timeScale;
        }
    }

    if (sizeScale != 1.0f)
    {
        // Rescale the "Scale" track
        ParticleSystem::Emitter::Track::KeyMap& track = emitter->tracks[ParticleSystem::TRACK_SCALE]->keys;;
        ParticleSystem::Emitter::Track::KeyMap  keys  = track;
        track.clear();
        for (ParticleSystem::Emitter::Track::KeyMap::const_iterator p = keys.begin(); p != keys.end(); p++)
        {
            track.insert(ParticleSystem::Emitter::Track::Key(p->time, p->value * sizeScale));
        }

        // Rescale position and speed groups
        DoRescaleGroup(emitter->groups[ParticleSystem::GROUP_POSITION], sizeScale);
        DoRescaleGroup(emitter->groups[ParticleSystem::GROUP_SPEED],    sizeScale);

        // Rescale speed, acceleration and size
        emitter->inwardSpeed        *= sizeScale;
        emitter->inwardAcceleration *= sizeScale;
        emitter->acceleration[0]    *= sizeScale;
        emitter->acceleration[1]    *= sizeScale;
        emitter->acceleration[2]    *= sizeScale;
        emitter->gravity            *= sizeScale;
        emitter->tailSize           *= sizeScale;
    }
}

bool RescaleEmitter(HWND hOwner, ParticleSystem::Emitter* emitter)
{
    // Show the rescale dialog
    RESCALE_OPTIONS options = {1.0f, 1.0f};
    if (!DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_RESCALE_EMITTER), hOwner, RescaleDialogFunc, (LPARAM)&options))
    {
        return false;
    }

    if (options.timeScale == 1.0f && options.sizeScale == 1.0f)
    {
        return false;
    }

    // Rescale the emitter
    DoRescaleEmitter(emitter, options.timeScale, options.sizeScale);
    return true;
}

bool RescaleParticleSystem(HWND hOwner, ParticleSystem* system)
{
    // Show the rescale dialog
    RESCALE_OPTIONS options = {1.0f, 1.0f};
    if (!DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_RESCALE_SYSTEM), hOwner, RescaleDialogFunc, (LPARAM)&options))
    {
        return false;
    }

    if (options.timeScale == 1.0f && options.sizeScale == 1.0f)
    {
        return false;
    }

    // Rescale the system
    vector<ParticleSystem::Emitter*>& emitters = system->getEmitters();
    for (size_t i = 0; i < emitters.size(); i++)
    {
        DoRescaleEmitter(emitters[i], options.timeScale, options.sizeScale);
    }

    return true;
}