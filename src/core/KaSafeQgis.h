#pragma once

#include <QString>

class QgsProject;

// QgsProject::read can ACCESS_VIOLATE on a broken .qgz (addMapLayers).
// Returns false on failure or SEH; never lets the process die.
bool kaSafeReadQgisProject(QgsProject* project, const QString& path);

// QgsProject::clear after a half-finished read can ACCESS_VIOLATE in
// QgsRelationManager::layersRemoved. Returns false on SEH.
bool kaSafeClearQgisProject(QgsProject* project);

// Count <layername> / layer-tree names that look like satellite basemaps.
// .qgz is unzipped without QgsProject::read. Returns -1 if the file exists
// but cannot be inspected.
int kaCountSatelliteLayersInQgisProjectFile(const QString& path);

// True when the companion project would recreate duplicate VWorld/Google
// satellite XYZ layers (the crash that started after "위성 중복 생성").
bool kaQgisProjectFileHasDuplicateSatellites(const QString& path);

// Session mark after QgsProject::read SEH/failure so the same .qgz is not
// opened again (half-loaded project + second read → RelationManager AV).
void kaMarkQgisProjectUnsafeToRead(const QString& path);
bool kaQgisProjectFileIsUnsafeToRead(const QString& path);
