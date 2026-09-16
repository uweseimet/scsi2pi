//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "command_localizer.h"
#include <cassert>

using enum LocalizationKey;

CommandLocalizer::CommandLocalizer()
{
    Add(ERROR_AUTHENTICATION, Language::EN, "Authentication failed");
    Add(ERROR_AUTHENTICATION, Language::DE, "Authentifizierung fehlgeschlagen");
    Add(ERROR_AUTHENTICATION, Language::FR, "Authentification éronnée");
    Add(ERROR_AUTHENTICATION, Language::ES, "Fallo de autentificación");

    Add(ERROR_OPERATION, Language::EN, "Unknown operation: {0}");
    Add(ERROR_OPERATION, Language::DE, "Unbekannte Operation: {0}");
    Add(ERROR_OPERATION, Language::FR, "Opération inconnue: {0}");
    Add(ERROR_OPERATION, Language::ES, "Operación desconocida: {0}");

    Add(ERROR_LOG_LEVEL, Language::EN, "Invalid log level '{0}'");
    Add(ERROR_LOG_LEVEL, Language::DE, "Ungültiger Log-Level '{0}'");
    Add(ERROR_LOG_LEVEL, Language::FR, "Niveau de journalisation invalide '{0}'");
    Add(ERROR_LOG_LEVEL, Language::ES, "Nivel de registro '{0}' no válido");

    Add(ERROR_MISSING_DEVICE_ID, Language::EN, "Missing device ID");
    Add(ERROR_MISSING_DEVICE_ID, Language::DE, "Fehlende Geräte-ID");
    Add(ERROR_MISSING_DEVICE_ID, Language::FR, "ID de périphérique manquante");
    Add(ERROR_MISSING_DEVICE_ID, Language::ES, "Falta el ID del dispositivo");

    Add(ERROR_MISSING_FILENAME, Language::EN, "Missing filename");
    Add(ERROR_MISSING_FILENAME, Language::DE, "Fehlender Dateiname");
    Add(ERROR_MISSING_FILENAME, Language::FR, "Nom de fichier manquant");
    Add(ERROR_MISSING_FILENAME, Language::ES, "Falta el nombre del archivo");

    Add(ERROR_DEVICE_MISSING_FILENAME, Language::EN, "Device {0} requires a filename");
    Add(ERROR_DEVICE_MISSING_FILENAME, Language::DE, "Gerät {0} benötigt einen Dateinamen");
    Add(ERROR_DEVICE_MISSING_FILENAME, Language::FR, "Périphérique {0} à besoin d'un nom de fichier");
    Add(ERROR_DEVICE_MISSING_FILENAME, Language::ES, "Dispositivo {0} requiere un nombre de archivo");

    Add(ERROR_IMAGE_IN_USE, Language::EN, "Image file '{0}' is already being used by device {1}");
    Add(ERROR_IMAGE_IN_USE, Language::DE, "Image-Datei '{0}' wird bereits von Gerät {1} benutzt");
    Add(ERROR_IMAGE_IN_USE, Language::FR,
        "Le fichier d'image '{0}' est déjà utilisé par périphérique {1}");
    Add(ERROR_IMAGE_IN_USE, Language::ES,
        "El archivo de imagen '{0}' ya está siendo utilizado por dispositivo {1}");

    Add(ERROR_IMAGE_FILE_INFO, Language::EN, "Can't create image file info for '{0}'");
    Add(ERROR_IMAGE_FILE_INFO, Language::DE,
        "Image-Datei-Information für '{0}' kann nicht erzeugt werden");
    Add(ERROR_IMAGE_FILE_INFO, Language::FR,
        "Ne peux pas créer les informations du fichier image '{0}'");
    Add(ERROR_IMAGE_FILE_INFO, Language::ES,
        "No se puede crear información de archivo de imagen para '{0}'");

    Add(ERROR_RESERVED_ID, Language::EN, "Device ID {0} is reserved");
    Add(ERROR_RESERVED_ID, Language::DE, "Geräte-ID {0} ist reserviert");
    Add(ERROR_RESERVED_ID, Language::FR, "ID de périphérique {0} réservée");
    Add(ERROR_RESERVED_ID, Language::ES, "El ID de dispositivo {0} está reservado");

    Add(ERROR_NON_EXISTING_UNIT, Language::EN, "Command for non-existing ID {0}, unit {1}");
    Add(ERROR_NON_EXISTING_UNIT, Language::DE, "Kommando für nicht existente ID {0}, Einheit {1}");
    Add(ERROR_NON_EXISTING_UNIT, Language::FR, "Command pour ID {0}, unité {1} non-existant");
    Add(ERROR_NON_EXISTING_UNIT, Language::ES, "Comando para ID {0} inexistente, unidad {1}");

    Add(ERROR_UNKNOWN_DEVICE_TYPE, Language::EN, "{0}:{1}: Unknown device type {2}");
    Add(ERROR_UNKNOWN_DEVICE_TYPE, Language::DE, "{0}:{1}: Unbekannter Gerätetyp {2}");
    Add(ERROR_UNKNOWN_DEVICE_TYPE, Language::FR, "{0}:{1}: Type de périphérique inconnu {2}");
    Add(ERROR_UNKNOWN_DEVICE_TYPE, Language::ES, "{0}:{1}: Tipo de dispositivo desconocido {2}");

    Add(ERROR_MISSING_DEVICE_TYPE, Language::EN,
        "{0}:{1}: Device type required for unknown extension of file '{2}'");
    Add(ERROR_MISSING_DEVICE_TYPE, Language::DE,
        "{0}:{1}: Gerätetyp erforderlich für unbekannte Extension der Datei '{2}'");
    Add(ERROR_MISSING_DEVICE_TYPE, Language::FR,
        "{0}:{1}: Type de périphérique requis pour extension inconnue du fichier '{2}'");
    Add(ERROR_MISSING_DEVICE_TYPE, Language::ES,
        "{0}:{1}: Tipo de dispositivo requerido para la extensión desconocida del archivo '{2}'");

    Add(ERROR_DUPLICATE_ID, Language::EN, "Duplicate ID {0}, unit {1}");
    Add(ERROR_DUPLICATE_ID, Language::DE, "Doppelte ID {0}, Einheit {1}");
    Add(ERROR_DUPLICATE_ID, Language::FR, "ID {0}, unité {1} dupliquée");
    Add(ERROR_DUPLICATE_ID, Language::ES, "ID duplicado {0}, unidad {1}");

    Add(ERROR_DETACH, Language::EN, "Couldn't detach device");
    Add(ERROR_DETACH, Language::DE, "Geräte konnte nicht entfernt werden");
    Add(ERROR_DETACH, Language::FR, "Impossible de détacher le périphérique");
    Add(ERROR_DETACH, Language::ES, "No se ha podido desconectar el dispositivo");

    Add(ERROR_EJECT_REQUIRED, Language::EN, "Existing medium must first be ejected");
    Add(ERROR_EJECT_REQUIRED, Language::DE, "Das vorhandene Medium muss erst ausgeworfen werden");
    Add(ERROR_EJECT_REQUIRED, Language::FR, "Media déjà existant doit d'abord être éjecté");
    Add(ERROR_EJECT_REQUIRED, Language::ES, "El medio existente debe ser expulsado primero");

    Add(ERROR_DEVICE_NAME_UPDATE, Language::EN, "Once set the device name cannot be changed anymore");
    Add(ERROR_DEVICE_NAME_UPDATE, Language::DE,
        "Ein bereits gesetzter Gerätename kann nicht mehr geändert werden");
    Add(ERROR_DEVICE_NAME_UPDATE, Language::FR,
        "Une fois défini, le nom de périphérique ne peut plus être changé");
    Add(ERROR_DEVICE_NAME_UPDATE, Language::ES,
        "Una vez establecido el nombre del dispositivo ya no se puede cambiar");

    Add(ERROR_SHUTDOWN_MODE_INVALID, Language::EN, "Invalid shutdown mode '{0}'");
    Add(ERROR_SHUTDOWN_MODE_INVALID, Language::DE, "Ungültiger Shutdown-Modus '{0}'");
    Add(ERROR_SHUTDOWN_MODE_INVALID, Language::FR, "Mode d'extinction invalide '{0}'");
    Add(ERROR_SHUTDOWN_MODE_INVALID, Language::ES, "Modo de apagado inválido '{0}'");

    Add(ERROR_SHUTDOWN_PERMISSION, Language::EN, "Missing root permission for shutdown or reboot");
    Add(ERROR_SHUTDOWN_PERMISSION, Language::DE,
        "Fehlende Root-Berechtigung für Shutdown oder Neustart");
    Add(ERROR_SHUTDOWN_PERMISSION, Language::FR,
        "Permissions root manquantes pour extinction ou redémarrage");
    Add(ERROR_SHUTDOWN_PERMISSION, Language::ES,
        "Falta el permiso de root para el apagado o el reinicio");

    Add(ERROR_FILE_OPEN, Language::EN, "Invalid or non-existing file '{0}'");
    Add(ERROR_FILE_OPEN, Language::DE, "Ungültige oder fehlende Datei '{0}'");
    Add(ERROR_FILE_OPEN, Language::FR, "Fichier invalide ou non-existant '{0}'");
    Add(ERROR_FILE_OPEN, Language::ES, "Archivo inválido o inexistente '{0}'");

    Add(ERROR_SCSI_LEVEL, Language::EN, "Invalid SCSI level: {0}");
    Add(ERROR_SCSI_LEVEL, Language::DE, "Ungültiger SCSI-Level: {0}");
    Add(ERROR_SCSI_LEVEL, Language::FR, "Niveau SCSI {0} invalide");
    Add(ERROR_SCSI_LEVEL, Language::ES, "Niveau SCSI {0} invalido");

    Add(ERROR_BLOCK_SIZE, Language::EN, "Invalid block size: {0} bytes");
    Add(ERROR_BLOCK_SIZE, Language::DE, "Ungültige Blockgröße: {0} Bytes");
    Add(ERROR_BLOCK_SIZE, Language::FR, "Taille de bloc {0} octets invalide");
    Add(ERROR_BLOCK_SIZE, Language::ES, "Tamaño de bloque {0} bytes invalido");

    Add(ERROR_BLOCK_SIZE_NOT_CONFIGURABLE, Language::EN,
        "Block size for device type {0} is not configurable");
    Add(ERROR_BLOCK_SIZE_NOT_CONFIGURABLE, Language::DE,
        "Blockgröße für Gerätetyp {0} ist nicht konfigurierbar");
    Add(ERROR_BLOCK_SIZE_NOT_CONFIGURABLE, Language::FR,
        "Taille de block pour le type de périphérique {0} non configurable");
    Add(ERROR_BLOCK_SIZE_NOT_CONFIGURABLE, Language::ES,
        "El tamaño del bloque para el tipo de dispositivo {0} no es configurable");

    Add(ERROR_CONTROLLER, Language::EN, "Couldn't create controller");
    Add(ERROR_CONTROLLER, Language::DE, "Controller konnte nicht erzeugt werden");
    Add(ERROR_CONTROLLER, Language::FR, "Impossible de créer le contrôleur");
    Add(ERROR_CONTROLLER, Language::ES, "No se ha podido crear el controlador");

    Add(ERROR_INVALID_ID, Language::EN, "Invalid device ID {0} (0-7)");
    Add(ERROR_INVALID_ID, Language::DE, "Ungültige Geräte-ID {0} (0-7)");
    Add(ERROR_INVALID_ID, Language::FR, "ID de périphérique invalide {0} (0-7)");
    Add(ERROR_INVALID_ID, Language::ES, "ID de dispositivo inválido {0} (0-7)");

    Add(ERROR_INVALID_LUN, Language::EN, "Invalid LUN {0} (0-{1})");
    Add(ERROR_INVALID_LUN, Language::DE, "Ungültige LUN {0} (0-{1})");
    Add(ERROR_INVALID_LUN, Language::FR, "LUN invalide {0} (0-{1})");
    Add(ERROR_INVALID_LUN, Language::ES, "LUN invalido {0} (0-{1})");

    Add(ERROR_MISSING_LUN0, Language::EN, "Missing LUN 0 for device ID {0}");
    Add(ERROR_MISSING_LUN0, Language::DE, "Fehlende LUN 0 für Geräte-ID {0}");
    Add(ERROR_MISSING_LUN0, Language::FR, "LUN 0 manquant pour l'ID de périphérique {0}");
    Add(ERROR_MISSING_LUN0, Language::ES, "Falta LUN 0 para la ID del dispositivo {0}");

    Add(ERROR_LUN0, Language::EN, "LUN 0 cannot be detached as long as there is still another LUN");
    Add(ERROR_LUN0, Language::DE,
        "LUN 0 kann nicht entfernt werden, solange noch eine andere LUN existiert");
    Add(ERROR_LUN0, Language::FR, "LUN 0 ne peux pas être détaché tant qu'il y'a un autre LUN");
    Add(ERROR_LUN0, Language::ES, "El LUN 0 no se puede desconectar mientras haya otro LUN");

    Add(ERROR_INITIALIZATION, Language::EN, "Initialization of {0} failed");
    Add(ERROR_INITIALIZATION, Language::DE, "Initialisierung von {0} fehlgeschlagen");
    Add(ERROR_INITIALIZATION, Language::FR, "Echec de l'initialisation de {0}");
    Add(ERROR_INITIALIZATION, Language::ES, "La inicialización del {0} falló");

    Add(ERROR_OPERATION_DENIED_STOPPABLE, Language::EN, "{0} operation denied, {1} isn't stoppable");
    Add(ERROR_OPERATION_DENIED_STOPPABLE, Language::DE,
        "{0}-Operation verweigert, {1} ist nicht stopbar");
    Add(ERROR_OPERATION_DENIED_STOPPABLE, Language::FR,
        "Opération {0} refusée, {1} ne peut être stoppé");
    Add(ERROR_OPERATION_DENIED_STOPPABLE, Language::ES,
        "{0} operación denegada, {1} no se puede parar");

    Add(ERROR_OPERATION_DENIED_REMOVABLE, Language::EN, "{0} operation denied, {1} isn't removable");
    Add(ERROR_OPERATION_DENIED_REMOVABLE, Language::DE,
        "{0}-Operation verweigert, {1} ist nicht wechselbar");
    Add(ERROR_OPERATION_DENIED_REMOVABLE, Language::FR,
        "Opération {0} refusée, {1} n'est pas détachable");
    Add(ERROR_OPERATION_DENIED_REMOVABLE, Language::ES, "{0} operación denegada, {1} no es removible");

    Add(ERROR_OPERATION_DENIED_PROTECTABLE, Language::EN,
        "{0} operation denied, {1} isn't protectable");
    Add(ERROR_OPERATION_DENIED_PROTECTABLE, Language::DE,
        "{0}-Operation verweigert, {1} ist nicht schützbar");
    Add(ERROR_OPERATION_DENIED_PROTECTABLE, Language::FR,
        "Opération {0} refusée, {1} n'est pas protégeable");
    Add(ERROR_OPERATION_DENIED_PROTECTABLE, Language::ES,
        "{0} operación denegada, {1} no es protegible");

    Add(ERROR_OPERATION_DENIED_READY, Language::EN, "{0} operation denied, {1} isn't ready");
    Add(ERROR_OPERATION_DENIED_READY, Language::DE, "{0}-Operation verweigert, {1} ist nicht bereit");
    Add(ERROR_OPERATION_DENIED_READY, Language::FR, "Opération {0} refusée, {1} n'est pas prêt");
    Add(ERROR_OPERATION_DENIED_READY, Language::ES, "{0} operación denegada, {1} no está listo");

    Add(ERROR_UNIQUE_SCDP, Language::EN, "There can only be a single SCDP device");
    Add(ERROR_UNIQUE_SCDP, Language::DE, "Es kann nur ein einziges SCDP-Gerät geben");
    Add(ERROR_UNIQUE_SCDP, Language::FR, "Il ne peut y avoir qu'un seul périphérique SCDP");
    Add(ERROR_UNIQUE_SCDP, Language::ES, "Sólo puede haber un único dispositivo SCDP");

    Add(ERROR_PERSIST, Language::EN, "Couldn't save '/etc/s2p.conf'");
    Add(ERROR_PERSIST, Language::DE, "'/etc/s2p.conf' konnte nicht gespeichert werden");
    Add(ERROR_PERSIST, Language::FR, "Impossible d'enregistrer '/etc/s2p.conf'");
    Add(ERROR_PERSIST, Language::ES, "No se pudo guardar '/etc/s2p.conf'");

    assert(localized_messages.size() == SUPPORTED_LOCALES.size());
}

void CommandLocalizer::Add(LocalizationKey key, Language language, string_view value)
{
    assert(!value.empty());

    auto &languages = localized_messages[language];
    assert(!languages.contains(key));

    languages[key] = value;
}

string CommandLocalizer::LocalizeImpl(LocalizationKey key, string_view locale, fmt::format_args args) const
{
    const Language lang = ToLanguage(locale);

    auto it = localized_messages.find(lang);
    if (it == localized_messages.end()) {
        it = localized_messages.find(Language::EN);
    }
    assert(it != localized_messages.end());

    const auto &msg_map = it->second;
    const auto m = msg_map.find(key);
    assert(m != msg_map.end());

    try {
        return fmt::vformat(m->second, args);
    } catch (const fmt::format_error&) {
        return m->second;
    }
}
