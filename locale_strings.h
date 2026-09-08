/* Von locale.py aus strings.cd erzeugt. NICHT von Hand aendern. */
#ifndef LOCALE_STRINGS_H
#define LOCALE_STRINGS_H

#define MSG_BT_REFRESH           0
#define MSG_BT_SELECT            1
#define MSG_BT_EDIT              2
#define MSG_BT_PREFS             3
#define MSG_BT_SUGGEST           4
#define MSG_BT_ALL               5
#define MSG_BT_NONE              6
#define MSG_BT_APPLY             7
#define MSG_BT_SAVE              8
#define MSG_BT_CLOSE             9
#define MSG_WIN_SELECT           10
#define MSG_WIN_PREFS            11
#define MSG_APP_DESCRIPTION      12
#define MSG_LBL_ADDRESS          13
#define MSG_LBL_TOKEN            14
#define MSG_LBL_INTERVAL         15
#define MSG_HINT_TOKEN           16
#define MSG_HINT_SELECT          17
#define MSG_STATUS_DEVICES       18
#define MSG_STATUS_SELECTED      19
#define MSG_STATUS_QUERYING      20
#define MSG_STATUS_SAVED         21
#define MSG_ERR_NOMEM            22
#define MSG_ERR_NOINTUITION      23
#define MSG_ERR_NOMUI            24
#define MSG_ERR_NOGUI            25
#define MSG_EMPTY_DASH           26
#define MSG_COVER_UP             27
#define MSG_COVER_STOP           28
#define MSG_COVER_DOWN           29
#define MSG_STATE_OPEN           30
#define MSG_STATE_CLOSED         31
#define MSG_STATE_MOTION         32
#define MSG_STATE_QUIET          33
#define MSG_STATE_WET            34
#define MSG_STATE_DRY            35
#define MSG_STATE_ON             36
#define MSG_STATE_OFF            37
#define MSG_STATE_OPENING        38
#define MSG_STATE_CLOSING        39
#define MSG_STATE_MISSING        40
#define MSG_GROUP_SWITCHES       41
#define MSG_GROUP_OPENINGS       42
#define MSG_GROUP_READINGS       43
#define MSG_GROUP_COVERS         44
#define MSG_CLI_ERROR            45
#define MSG_CLI_ENTRIES          46
#define MSG_CLI_HIDDEN           47
#define MSG_CLI_CATALOG          48
#define MSG_CLI_NOTHING          49
#define MSG_CLI_CREATED          50
#define MSG_CLI_WRITEFAIL        51
#define MSG_CLI_READFAIL         52
#define MSG_CLI_READBACK         53
#define MSG_CLI_ICON             54
#define MSG_CLI_POLLED           55
#define MSG_CLI_PREFSSAVED       56
#define MSG_ERR_NONE             57
#define MSG_ERR_NOPREFS          58
#define MSG_ERR_NOHOST           59
#define MSG_ERR_NOTOKEN          60
#define MSG_ERR_PREFSWRITE       61
#define MSG_ERR_IMPORTWRITE      62
#define MSG_ERR_NOSOCKET         63
#define MSG_ERR_NORESOLVE        64
#define MSG_ERR_SOCKET           65
#define MSG_ERR_REFUSED          66
#define MSG_ERR_SEND             67
#define MSG_ERR_RECV             68
#define MSG_ERR_BADRESPONSE      69
#define MSG_ERR_NOBODY           70
#define MSG_ERR_TOKEN401         71
#define MSG_ERR_HTTP             72
#define MSG_ERR_NODEVICES        73
#define MSG_ERR_NOMEMLIST        74
#define MSG_ERR_BADENTITY        75
#define MSG_FILE_PREFS_HEAD      76
#define MSG_FILE_PREFS_NOTE      77
#define MSG_FILE_PREFS_POLL      78
#define MSG_FILE_IMPORT_HEAD     79
#define MSG_FILE_DASH_HEAD       80
#define MSG_FILE_DASH_NOTE       81
#define MSG_FILE_DASH_KINDS      82
#define MSG_ED_TITLE             83
#define MSG_ED_ADDDEVICE         84
#define MSG_ED_PAGES             85
#define MSG_ED_CONTENT           86
#define MSG_ED_SELECTED          87
#define MSG_ED_NEW               88
#define MSG_ED_REMOVE            89
#define MSG_ED_UP                90
#define MSG_ED_DOWN              91
#define MSG_ED_GROUP             92
#define MSG_ED_DEVICE            93
#define MSG_ED_RENAME            94
#define MSG_ED_SET               95
#define MSG_ED_ADD               96
#define MSG_ED_LBL_NAME          97
#define MSG_ED_LBL_ICON          98
#define MSG_ED_LBL_KIND          99
#define MSG_ED_NEWPAGE           100
#define MSG_ED_NEWGROUP          101
#define MSG_ED_DEVICES           102
#define MSG_KIND_TOGGLE          103
#define MSG_KIND_LAMP            104
#define MSG_KIND_VALUE           105
#define MSG_KIND_GAUGE           106
#define MSG_KIND_COVER           107
#define MSG_KIND_TEXT            108
#define MSG_ICON_OFFICE          109
#define MSG_ICON_BATH            110
#define MSG_ICON_ATTIC           111
#define MSG_ICON_GARAGE          112
#define MSG_ICON_GARDEN          113
#define MSG_ICON_CELLAR          114
#define MSG_ICON_KITCHEN         115
#define MSG_ICON_BEDROOM         116
#define MSG_ICON_TOILET          117
#define MSG_ICON_STAIRS          118
#define MSG_ICON_LAUNDRY         119
#define MSG_ICON_LIVING          120
#define MSG_ICON_NOAREA          121
#define MSG_ICON_HOUSE           122
#define MSG_ICON_OVERVIEW        123
#define MSG_ICON_ENERGY          124
#define MSG_ICON_SOLAR           125
#define MSG_ICON_BATTERY         126
#define MSG_ICON_TEMPERATURE     127
#define MSG_ICON_HUMIDITY        128
#define MSG_ICON_WEATHER         129
#define MSG_ICON_LIGHT           130
#define MSG_ICON_SOCKET          131
#define MSG_ICON_WINDOW          132
#define MSG_ICON_DOOR            133
#define MSG_ICON_LOCK            134
#define MSG_ICON_SECURITY        135
#define MSG_ICON_CAR             136
#define MSG_ICON_TV              137
#define MSG_ICON_MUSIC           138
#define MSG_ICON_NETWORK         139
#define MSG_ICON_BLIND           140
#define MSG_ICON_FAN             141
#define MSG_ICON_TIME            142
#define MSG_ICON_TOOL            143
#define MSG_ICON_PRINTER         144
#define MSG_CLI_SETUPHINT        145
#define MSG_CLI_SETUPCMD         146
#define MSG_CLI_TURNEDON         147
#define MSG_CLI_TURNEDOFF        148
#define MSG_CLI_TOGGLED          149
#define MSG_ERR_NOHTTPS          150
#define MSG_ERR_TIMEOUT          151
#define MSG_KIND_CLIMATE         152
#define MSG_GROUP_CLIMATE        153
#define MSG_HVAC_OFF             154
#define MSG_HVAC_HEAT            155
#define MSG_HVAC_COOL            156
#define MSG_HVAC_AUTO            157
#define MSG_HVAC_DRY             158
#define MSG_HVAC_FAN             159
#define MSG_HVAC_HEATCOOL        160

#define AMILOC_COUNT 161

/* Die eingebauten Texte sind Englisch - siehe amiloc.h. */
static const char *const AMILOC_BUILTIN[AMILOC_COUNT] = {
    /* 0   */ "_Refresh",
    /* 1   */ "S_elect ...",
    /* 2   */ "_Edit ...",
    /* 3   */ "Se_ttings ...",
    /* 4   */ "S_uggest",
    /* 5   */ "A_ll",
    /* 6   */ "_None",
    /* 7   */ "_Apply",
    /* 8   */ "_Save",
    /* 9   */ "_Close",
    /* 10  */ "Select devices",
    /* 11  */ "Settings",
    /* 12  */ "Control Home Assistant from your Amiga",
    /* 13  */ "_Address",
    /* 14  */ "_Token",
    /* 15  */ "_Interval (s)",
    /* 16  */ "Token: in Home Assistant under Profile, Security.",
    /* 17  */ "Clicking a row adds or removes it.",
    /* 18  */ "%ld devices on %ld pages",
    /* 19  */ "%ld of %ld selected",
    /* 20  */ "Querying Home Assistant ...",
    /* 21  */ "Saved.",
    /* 22  */ "not enough memory",
    /* 23  */ "cannot open intuition.library",
    /* 24  */ "cannot open muimaster.library - is MUI installed?",
    /* 25  */ "cannot build the user interface.",
    /* 26  */ "\33cNo dashboards yet.\n\nUse Select to adopt devices -\nthat creates one page per room.",
    /* 27  */ "Up",
    /* 28  */ "Stop",
    /* 29  */ "Down",
    /* 30  */ "open",
    /* 31  */ "closed",
    /* 32  */ "motion",
    /* 33  */ "quiet",
    /* 34  */ "wet",
    /* 35  */ "dry",
    /* 36  */ "on",
    /* 37  */ "off",
    /* 38  */ "opening",
    /* 39  */ "closing",
    /* 40  */ "\33rmissing",
    /* 41  */ "Switches",
    /* 42  */ "Windows and doors",
    /* 43  */ "Readings",
    /* 44  */ "Blinds",
    /* 45  */ "Error: %s\n",
    /* 46  */ "\n%d of %d entries",
    /* 47  */ ", %d hidden (use ALL to show)",
    /* 48  */ "Catalog: %d entries, %d adopted\n\n",
    /* 49  */ "Nothing adopted - select devices in the GUI first.\n",
    /* 50  */ "created: %d pages from %d adopted devices\n",
    /* 51  */ "Write failed.\n",
    /* 52  */ "Read failed.\n",
    /* 53  */ "read back: %d pages\n\n",
    /* 54  */ "%s  (icon %d)\n",
    /* 55  */ "\npolled on the interval: %d devices\n",
    /* 56  */ "Settings saved.\n",
    /* 57  */ "no error",
    /* 58  */ "no settings found (ENVARC:AmiHomeassist/AmiHomeassist.prefs)",
    /* 59  */ "host= is missing from the settings",
    /* 60  */ "token= is missing from the settings",
    /* 61  */ "cannot write the settings",
    /* 62  */ "cannot write the adopted-device list",
    /* 63  */ "bsdsocket.library not available - is the TCP stack up?",
    /* 64  */ "cannot resolve the host name",
    /* 65  */ "socket() failed",
    /* 66  */ "connection refused - is Home Assistant running?",
    /* 67  */ "sending failed",
    /* 68  */ "receiving failed",
    /* 69  */ "unintelligible response from the server",
    /* 70  */ "response without a body",
    /* 71  */ "Home Assistant rejects the token (401)",
    /* 72  */ "Home Assistant answers with HTTP %d",
    /* 73  */ "Home Assistant returns no devices",
    /* 74  */ "not enough memory for the device list",
    /* 75  */ "unusable entity ID",
    /* 76  */ "; AmiHomeassist - settings\n",
    /* 77  */ "; Lines starting with ; are comments. Format: key=value\n\n",
    /* 78  */ "\n; Interval of the state query, in seconds\n",
    /* 79  */ "; AmiHomeassist - adopted devices, one ID per line\n",
    /* 80  */ "; AmiHomeassist - dashboards\n",
    /* 81  */ "; Written by the editor. Edit by hand if you must:\n",
    /* 82  */ "; Kinds: toggle lamp value gauge cover text\n\n",
    /* 83  */ "Edit dashboards",
    /* 84  */ "Add device",
    /* 85  */ "\33cPages",
    /* 86  */ "\33cPage content",
    /* 87  */ "Selection",
    /* 88  */ "New",
    /* 89  */ "Remove",
    /* 90  */ "Up",
    /* 91  */ "Down",
    /* 92  */ "Group",
    /* 93  */ "Device ...",
    /* 94  */ "Rename",
    /* 95  */ "set",
    /* 96  */ "_Add",
    /* 97  */ "_Name",
    /* 98  */ "S_ymbol",
    /* 99  */ "_Kind",
    /* 100 */ "New page",
    /* 101 */ "New group",
    /* 102 */ "Devices",
    /* 103 */ "Switch",
    /* 104 */ "Indicator",
    /* 105 */ "Number",
    /* 106 */ "Bar",
    /* 107 */ "Blind",
    /* 108 */ "Text",
    /* 109 */ "Office",
    /* 110 */ "Bathroom",
    /* 111 */ "Attic",
    /* 112 */ "Garage",
    /* 113 */ "Garden",
    /* 114 */ "Cellar",
    /* 115 */ "Kitchen",
    /* 116 */ "Bedroom",
    /* 117 */ "Toilet",
    /* 118 */ "Stairs",
    /* 119 */ "Laundry",
    /* 120 */ "Living room",
    /* 121 */ "No room",
    /* 122 */ "House",
    /* 123 */ "Overview",
    /* 124 */ "Energy",
    /* 125 */ "Solar",
    /* 126 */ "Battery",
    /* 127 */ "Temperature",
    /* 128 */ "Humidity",
    /* 129 */ "Weather",
    /* 130 */ "Light",
    /* 131 */ "Socket",
    /* 132 */ "Window",
    /* 133 */ "Door",
    /* 134 */ "Lock",
    /* 135 */ "Security",
    /* 136 */ "Car",
    /* 137 */ "Television",
    /* 138 */ "Music",
    /* 139 */ "Network",
    /* 140 */ "Blind",
    /* 141 */ "Fan",
    /* 142 */ "Time",
    /* 143 */ "Tool",
    /* 144 */ "Printer",
    /* 145 */ "\nSet it up like this:\n",
    /* 146 */ "  AmiHomeassist HOST=http://homeassistant:8123 TOKEN=<your-token> SAVE\n",
    /* 147 */ "turned on",
    /* 148 */ "turned off",
    /* 149 */ "toggled",
    /* 150 */ "https is not supported - please use http://",
    /* 151 */ "Home Assistant does not answer - timed out",
    /* 152 */ "Thermostat",
    /* 153 */ "Heating",
    /* 154 */ "off",
    /* 155 */ "heat",
    /* 156 */ "cool",
    /* 157 */ "auto",
    /* 158 */ "dry",
    /* 159 */ "fan",
    /* 160 */ "heat/cool",
};

#endif
