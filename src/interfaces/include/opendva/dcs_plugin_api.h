/* Plugin C ABI (README §13). Mirrors the original 3DCS User-DLL registration
 * model so third-party Move/Tolerance/Measure routines can be loaded.
 * Pure C linkage: stable across compilers. */
#ifndef OPENDVA_PLUGIN_API_H
#define OPENDVA_PLUGIN_API_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    dcsCalTypeIntern = 0,
    dcsCalTypeMove = 1,
    dcsCalTypeMoveDlg = 2,
    dcsCalTypeTole = 3,
    dcsCalTypeToleDlg = 4,
    dcsCalTypeMeas = 5,
    dcsCalTypeMeasDlg = 6,
    dcsCalTypeExec = 7
} dcsCalType;

/* Opaque pointer to the calculation struct; the routine casts it to the
 * concrete wpMOVECAL_s / wpTOLECAL_s / wpMEASCAL_s (README §13.4). */
typedef void* dcsDataPtr;
typedef void (*dcsCalFuncPtr)(dcsDataPtr pCalData);

/* Minimal measure calculation payload for User-DLL measure routines in the
 * reference build. The host initialises value to 0; the routine writes the
 * measured scalar result. */
typedef struct dcsMeasureCalData {
    double value;
} dcsMeasureCalData;

/* Minimal move calculation payload for User-DLL move routines. The host
 * initialises transform to identity; the routine writes the rigid transform. */
typedef struct dcsMoveCalData {
    double transform[3][4];
} dcsMoveCalData;

/* Implemented by the host (A8). A plugin's init entry calls this once per
 * routine; name is shown in the UI dropdown. */
void dcsApiRegisterCalFunc(const char* name, dcsCalFuncPtr fn, dcsCalType type);
void dcsApiRemoveCalFunc(const char* name, dcsCalType type);

/* Host services available to plugins. */
void dcsApiLogWrite(const char* str);
void dcsApiDisplayHint(const char* hint);
void dcsApiDisplayMsg(const char* msg);

/* Plugins MUST export these two entry points (resolved by name on load). */
/*   void dcsDLLInit(void);   -- register routines here
 *   void dcsDLLExit(void);   -- remove routines here */

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* OPENDVA_PLUGIN_API_H */
