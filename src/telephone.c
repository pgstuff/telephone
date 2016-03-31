/*
Copyright (c) 2016, PostgreSQL Global Development Group
*/

#include "postgres.h"
#include "fmgr.h"
#include "libpq/pqformat.h"             /* needed for send/recv functions */
#include "access/hash.h"                /* needed for hash_any function */
#include "utils/builtins.h"             /* needed for cstring_to_text function */
// Enum:
#include "catalog/namespace.h"
#include "catalog/pg_enum.h"
#include "utils/syscache.h"
#include "utils/catcache.h"
//#include "funcapi.h"
//#include "mb/pg_wchar.h"
//#include "utils/lsyscache.h"
#include "access/htup_details.h"
#include "utils/array.h"
// Array:
#include "catalog/pg_type.h"
#include "utils/lsyscache.h"

PG_MODULE_MAGIC;

#define MAX_DIGITS 64
#define MAX_STORAGE_HEADER 4

// For subscriber formatting: space group number, list of next groups with the number indicating the number of spaces in the groups.
// Last used:
// grep '^#define STATE_' src/telephone.c | grep -v '^99' | tr -s ' ' | cut -d ' ' -f 3 | grep -v '^99' | sort | tail -n 1
#define STATE_ERROR                     501
#define STATE_START                     502
#define STATE_DIGITS_IGNORE_QUALIFIER   503
#define STATE_DIGITS                    504
#define STATE_CALLING_CODE_START        505 // Calling code states must have this value or higher.
#define STATE_NANP_NPA_START            506
#define STATE_NANP_NPA_DIGITS           507
#define STATE_NANP_COC_START            508
#define STATE_NANP_COC_DIGITS           509
#define STATE_NANP_SUB_START            510
#define STATE_NANP_SUB_DIGITS           511
#define STATE_CALLING_CODE_ZONE2        513
#define STATE_CALLING_CODE_ZONE3        514
#define STATE_CALLING_CODE_ZONE4        515
#define STATE_CALLING_CODE_ZONE5        516
#define STATE_CALLING_CODE_ZONE6        517
#define STATE_RUSSIA_AREA_CODE_START    518
#define STATE_CALLING_CODE_ZONE8        519
#define STATE_CALLING_CODE_ZONE9        520
#define STATE_CALLING_CODE_35           524
#define STATE_FINLAND_AREA_START        525
#define STATE_FINLAND_AREA_DIGITS       526
#define STATE_FINLAND_SUB_1_3_2_2       527
#define STATE_FINLAND_SUB_2_2_2         528
#define STATE_FINLAND_SUB_3_2           529
#define STATE_FINLAND_SUB_1_3_3         530
#define STATE_FINLAND_SUB_2_3           568
#define STATE_UK_AREA_START             537
#define STATE_UK_AREA_DIGIT2            538
#define STATE_UK_AREA_DIGIT3            569
#define STATE_UK_AREA_DIGIT4            570
#define STATE_UK_AREA_DIGIT5            571
#define STATE_UK_SUB_1_3_4              540 // Sub group index, sub group 1 len, sub group 2 len.
#define STATE_UK_SUB_1_4_4              542
#define STATE_UK_SUB_1_5                544
#define STATE_UK_SUB_2_4                546 // Sub group index, sub group len
#define STATE_AUSTRALIA_AREA_START      547
#define STATE_AUSTRALIA_SUB_1_4_4       548
#define STATE_AUSTRALIA_SUB_2_4         549
#define STATE_AUSTRALIA_AREA_1_3_3_3    577
#define STATE_AUSTRALIA_SUB_1_3_3       579
#define STATE_AUSTRALIA_SUB_2_3         578
#define STATE_RUSSIA_AREA_CODE_DIGITS   550
#define STATE_RUSSIA_ZONE_START         551
#define STATE_RUSSIA_ZONE_DIGITS        552
#define STATE_RUSSIA_SUB                553
#define STATE_UK_SUB_1_6                554
#define STATE_UK_AREA_1_4_6             555
#define STATE_UK_SUB_6                  556
#define STATE_INDIA_AREA_START          557
#define STATE_INDIA_AREA_DIGIT2         558
#define STATE_INDIA_AREA_DIGIT3         575
#define STATE_INDIA_AREA_DIGIT4         576
#define STATE_INDIA_SUB_1_4_4           559
#define STATE_INDIA_SUB_1_3_4           560
#define STATE_INDIA_SUB_1_2_4           561
#define STATE_INDIA_SUB_2_4             562
#define STATE_INDIA_SUB_1_6             563
#define STATE_JAPAN_AREA_START          531
#define STATE_JAPAN_AREA_DIGIT_2        532
#define STATE_JAPAN_SUB_1_4_4           533
#define STATE_JAPAN_SUB_1_3_4           534
#define STATE_JAPAN_SUB_1_2_4           535
#define STATE_JAPAN_SUB_1_1_4           536
#define STATE_JAPAN_SUB_1_6             564
#define STATE_JAPAN_SUB_2_4             565
#define STATE_JAPAN_SUB_1_3_3           566
#define STATE_JAPAN_SUB_2_3             567
#define STATE_JAPAN_AREA_DIGIT3         572
#define STATE_JAPAN_AREA_DIGIT4         573
#define STATE_JAPAN_AREA_DIGIT5         574
#define STATE_FRANCE_START              580
#define STATE_FRANCE_DIGITS             581
#define STATE_ITALY_AREA_START          582
#define STATE_ITALY_AREA_DIGIT2         583
#define STATE_ITALY_AREA_DIGIT3         584
#define STATE_ITALY_AREA_DIGIT4         585
#define STATE_ITALY_SUB_1_8             586
#define STATE_ITALY_SUB_1_7             587
#define STATE_ITALY_SUB_1_6             588
#define STATE_ITALY_CELL_PREFIX         589
#define STATE_ITALY_EX                  590
#define STATE_ITALY_SUB_1_4             591
#define STATE_ITALY_SUB_2_3_3           592
#define STATE_ITALY_SUB_2_3             593
#define STATE_EXTENSION_START           998 // Value must be higher than all calling code states.
#define STATE_EXTENSION_DIGITS          999

#define ALT_STATE_NULL             0
#define ALT_STATE_JAPAN_LAND_2  2000
#define ALT_STATE_JAPAN_LAND_3  2001
#define ALT_STATE_JAPAN_LAND_4  2002
#define ALT_STATE_UK_4          2003

#define ERROR_PARSE_STATE_OR_OTHER              201
#define ERROR_CALLING_CODE_START_INVALID        202
#define ERROR_CALLING_CODE_DIGITS_INVALID       203
#define ERROR_AREA_CODE_START_INVALID           204
#define ERROR_AREA_CODE_DIGITS_INVALID          205
#define ERROR_EXCHANGE_CODE_START_INVALID       206
#define ERROR_EXCHANGE_CODE_DIGITS_INVALID      207
#define ERROR_SUBSCRIBER_NUM_START_INVALID      208
#define ERROR_SUBSCRIBER_NUM_DIGITS_INVALID     209
#define ERROR_SUBSCRIBER_NUM_START2_INVALID     215
#define ERROR_EXTENSION_START_INVALID           210
#define ERROR_EXTENSION_DIGITS_INVALID          211
#define ERROR_DIGITS_INVALID                    212
#define ERROR_NEED_LENGTH_CODE                  213
#define ERROR_AREA_CODE_NOT_FOUND               214
#define ERROR_NUMBER_OBSOLETE                   216
#define ERROR_CALLING_CODE_NEED_CODE            217
#define ERROR_INVALID_CHAR                      218
#define ERROR_INVALID_STATE                     219

#define DIGIT_BIN_OFFSET                -4
#define DIGIT_BIN_SPECIAL               0
#define DIGIT_BIN_HEX_SPECIAL_SPECIAL   '\x00' // Used for mask indicator.
#define DIGIT_SPECIAL   -4 // Multiple meanings:  Start: Digits mode, Middle: Whitespace, End: Letter mask
#define DIGIT_SLASH     -3 // Indicates alternative numbers.  Only valid in digits mode or in the extension.
#define DIGIT_CONFIRM   -2 // Prompt (confirmation pause) using ";"  Phones also use w.
#define DIGIT_PAUSE     -1 // w = .5 sec pause in PBXs and prompt in phones.  Phones also use p and , for a 2 sec pause.
#define DIGIT_STAR      10
#define DIGIT_POUND     11
#define DIGIT_PLUS      12 // Never stored.  + is implied when DIGIT_SPECIAL is not at the start.

// grep '^#define FIELD_' src/telephone.c | tr -s ' ' | cut -d ' ' -f 3 | grep -v '^99' | sort | tail -n 1
#define FIELD_INVALID                   20
#define FIELD_CALLING_CODE              21
#define FIELD_NANP_NPAC                 24
#define FIELD_NANP_COC                  25
#define FIELD_NANP_SUB                  26
#define FIELD_DIGITS_QUALIFIER          29 // Not a user visible field.
#define FIELD_DIGITS                    30 // Digits mode (just a bunch of digits).
#define FIELD_EXTENSION                 31
#define FIELD_JAPAN_AREA_CODE           32
#define FIELD_JAPAN_SUB1                33
#define FIELD_JAPAN_SUB2                34
#define FIELD_FINLAND_AREA_CODE         35
#define FIELD_UK_AREA_CODE              36
#define FIELD_UK_SUB1                   37
#define FIELD_UK_SUB2                   38
#define FIELD_AUSTRALIA_AREA_CODE       39
#define FIELD_AUSTRALIA_SUB1            40
#define FIELD_AUSTRALIA_SUB2            41
#define FIELD_RUSSIA_AREA_CODE          42
#define FIELD_RUSSIA_ZONE_CODE          43
#define FIELD_RUSSIA_SUB1               44
#define FIELD_RUSSIA_SUB2               45
#define FIELD_INDIA_AREA_CODE           46
#define FIELD_INDIA_SUB1                47
#define FIELD_INDIA_SUB2                48
#define FIELD_FINLAND_SUB1              49
#define FIELD_FINLAND_SUB2              50
#define FIELD_FINLAND_SUB3              51
#define FIELD_FRANCE_AREA_CODE          57
#define FIELD_FRANCE_SUB                52
#define FIELD_ITALY_AREA_CODE           53
#define FIELD_ITALY_EX_CODE             54
#define FIELD_ITALY_SUB1                55
#define FIELD_ITALY_SUB2                56

#define VALUEMASK_LETTER              0b00000111
#define VALUEMASK_WHITESPACE          0b00001000
#define VALUEMASK_PAUSE               0b01110000
#define VALUEMASK_CONFIRM             0b10000000
#define VALUEBIT_WHITESPACE             3
#define VALUEBIT_PAUSE                  4
#define VALUEBIT_CONFIRM                7
#define VALUEPARTMASK_PAUSE           0b111
#define VALUEPARTMASK_LETTER          0b111

#define WHITESPACE_IS_DATA              0
#define CONFIRM_IS_DATA                 0
#define PAUSE_IS_DATA                   0

#define CALLING_CODE_FORMATTING_MAX_LEN 7

typedef struct
{
    int         index;
    const char *label;
}   EnumLabel;

void getEnumLabelOids(const char *typname, EnumLabel labels[], Oid oid_out[], int count);

enum TelephoneFormat {TFORM_INTERNATIONAL, TFORM_DOMESTIC, TFORM_DIGITS_ONLY, TFORM_FORMAT_COUNT};

static EnumLabel telephone_format_labels[TFORM_FORMAT_COUNT] =
    {
        {TFORM_INTERNATIONAL,   "international"},
        {TFORM_DOMESTIC,        "domestic"},
        {TFORM_DIGITS_ONLY,     "digits_only"}
    };

enum TelephoneSubfield {TSF_DIGITS, TSF_CALLING_CODE,
    TSF_GROUP1, TSF_GROUP2, TSF_GROUP3, TSF_GROUP4,
    TSF_SUBSCRIBER, TSF_EXTENSION, TSF_SUBFIELD_COUNT};

static EnumLabel telephone_subfield_labels[TSF_SUBFIELD_COUNT] =
    {
        {TSF_DIGITS,        "digits"},
        {TSF_CALLING_CODE,  "calling_code"},
        {TSF_GROUP1,        "group1"},
        {TSF_GROUP2,        "group2"},
        {TSF_GROUP3,        "group3"},
        {TSF_GROUP4,        "group4"},
        {TSF_SUBSCRIBER,    "subscriber"},
        {TSF_EXTENSION,     "extension"}
    };

// TelephoneService does not store fictitious status.  Fictitious numbers need to use the same service types.
enum TelephoneService {
    TSERV_UNKNOWN_UNKNOWN,  // The code does not know the service type.
    TSERV_KNOWN_UNKNOWN,    // The number plan indicates that the service type is unknowable.
    TSERV_LAND, TSERV_OTHER_GEO, TSERV_LAND_OR_CELL, TSERV_CELL, TSERV_PAGER, TSERV_VOIP, TSERV_FOLLOW_ME,
    TSERV_TOLLFREE, TSERV_SHARED_COST, TSERV_CALLER_COST, TSERV_CHARGE,
    TSERV_CARRIER_SERVICES_INTRA, TSERV_CARRIER_SERVICES_INTERNATIONAL, TSERV_GOVERNMENT,
    TSERV_VOICEMAIL, TSERV_OTHER, TSERV_SERVICE_COUNT,
    TSERV_NEED_TO_PARSE,    // The parser has not read enough digits to know the service type.
    TSERV_QUERY_RUSSIA};    // Query functions are required if there is any computation cost to know the service type.

static EnumLabel telephone_service_labels[TSERV_SERVICE_COUNT] =
    {
        {TSERV_UNKNOWN_UNKNOWN, "unknown_unknown"},
        {TSERV_KNOWN_UNKNOWN,   "known_unknown"},
        {TSERV_LAND,            "land"},
        {TSERV_OTHER_GEO,       "other_geo"},
        {TSERV_LAND_OR_CELL,    "land_or_cell"},
        {TSERV_CELL,            "cell"},
        {TSERV_PAGER,           "pager"},
        {TSERV_VOIP,            "voip"},
        {TSERV_FOLLOW_ME,       "follow_me"},
        {TSERV_TOLLFREE,        "tollfree"},
        {TSERV_SHARED_COST,     "shared_cost"},
        {TSERV_CALLER_COST,     "caller_cost"},
        {TSERV_CHARGE,          "charge"},
        {TSERV_CARRIER_SERVICES_INTRA,"carrier_services_intra"}, // Unique services accessible from within the carrier's network.
        {TSERV_CARRIER_SERVICES_INTERNATIONAL,"carrier_services_international"},
        {TSERV_GOVERNMENT,      "government"},
        {TSERV_VOICEMAIL,       "voicemail"},
        {TSERV_OTHER,           "other"}
    };

enum TelephoneFictitious {
    TFICT_REAL,                     // No fictitious attributes.
    TFICT_REAL_MISTAKEN,            // The public may mistake this as fictitious, but this can be in use by a real service.
    TFICT_REAL_FICTITIOUS,          // Owned for fictitious use, but it could be sold for real use at a later time.
    TFICT_FICTITIOUS_UNRESERVED,    // Recognized as fictitious, has no service, but this may change in the future.
    TFICT_FICTITIOUS_RESERVED,      // Officially allocated for fictitious use only.
    TFICT_FICTITIOUS_COUNT};

static EnumLabel telephone_fictitious_labels[TFICT_FICTITIOUS_COUNT] =
    {
        {TFICT_REAL,                    "real"},
        {TFICT_REAL_MISTAKEN,           "real_mistaken"},
        {TFICT_REAL_FICTITIOUS,         "real_fictitious"},
        {TFICT_FICTITIOUS_UNRESERVED,   "fictitious_unreserved"},
        {TFICT_FICTITIOUS_RESERVED,     "fictitious_reserved"}
    };

enum TelephoneMode {TMODE_CALLING_CODE, TMODE_DIGITS, TMODE_MODE_COUNT};

static EnumLabel telephone_mode_labels[TMODE_MODE_COUNT] =
    {
        {TMODE_CALLING_CODE,    "calling_code"},
        {TMODE_DIGITS,          "digits"}
    };

struct digit_letter {
    int value;
    uint letter_index;
    char invalid_char;
};

struct parse_buffer {
    uint parse_state;
    uint alt_state; // Used for when ranges with more digits overlap with different ranges with smaller digits.
    uint digit_pos_next;
    int digit_values[MAX_DIGITS];       // Used for determining identity/equality.
    uint digit_valuesmask[MAX_DIGITS];  // Not used for determining identity/equality.
    uint digit_fields[MAX_DIGITS];      // Used for text formatting.  Not accurate enough for being exposed in a public function.
    uint field_pos;
    uint field_value; // Only populated when needed for branching.
    enum TelephoneService service_type;
    uint error_code;
    uint code_id; // A unique ID for locating the code that set the error.
};

/*extern Datum  telephone_eq      (PG_FUNCTION_ARGS);
extern Datum  telephone_ne      (PG_FUNCTION_ARGS);
extern Datum  telephone_gt      (PG_FUNCTION_ARGS);
extern Datum  telephone_ge      (PG_FUNCTION_ARGS);
extern Datum  telephone_lt      (PG_FUNCTION_ARGS);
extern Datum  telephone_le      (PG_FUNCTION_ARGS);
extern Datum  telephone_cmp     (PG_FUNCTION_ARGS);
extern Datum  telephone_hash    (PG_FUNCTION_ARGS);
extern Datum  telephone_smaller (PG_FUNCTION_ARGS);
extern Datum  telephone_larger  (PG_FUNCTION_ARGS);*/

/*
Optimizations:

1) Sorting.  Because it occurs in a tight loop, this needs to be the first optimization.  This affects the storage format.

2) Default text in and out.  Because it is used for pg_dump and restore, it needs to be fast in order to avoid slowing down
   database maintenance operations.  This requires that the parse function be optimized.

3) Complexity budget.  Distribute complexity and keep the code approachable.

Technically, service_type should not be in the parse path, but the cost is negligible and it helps keep the complexity down.
*/

Oid *format_oids        = 0;
Oid *subfield_oids      = 0;
Oid *service_oids       = 0;
Oid *fictitious_oids    = 0;
Oid *mode_oids          = 0;

/*
int16   typlen_text;
bool    typlen_text_typbyval;
char    typlen_text_typalign = 0;*/

Oid     typlen_telephone_oid = 0;
int16   typlen_telephone;
bool    typlen_telephone_typbyval;
char    typlen_telephone_typalign;

static void init_oids_format() {
    if (format_oids == 0) {
        format_oids = malloc(TFORM_FORMAT_COUNT * sizeof(Oid));
        getEnumLabelOids("telephone_format", telephone_format_labels, format_oids, TFORM_FORMAT_COUNT);
    }
}

static void init_oids_subfield() {
    if (subfield_oids == 0) {
        subfield_oids = malloc(TSF_SUBFIELD_COUNT * sizeof(Oid));
        getEnumLabelOids("telephone_subfield", telephone_subfield_labels, subfield_oids, TSF_SUBFIELD_COUNT);
    }
}

static void init_oids_service() {
    if (service_oids == 0) {
        service_oids = malloc(TSERV_SERVICE_COUNT * sizeof(Oid));
        getEnumLabelOids("telephone_service", telephone_service_labels, service_oids, TSERV_SERVICE_COUNT);
    }
}

static void init_oids_fictitious() {
    if (fictitious_oids == 0) {
        fictitious_oids = malloc(TFICT_FICTITIOUS_COUNT * sizeof(Oid));
        getEnumLabelOids("telephone_fictitious", telephone_fictitious_labels, fictitious_oids, TFICT_FICTITIOUS_COUNT);
    }
}

static void init_oids_mode() {
    if (mode_oids == 0) {
        mode_oids = malloc(TMODE_MODE_COUNT * sizeof(Oid));
        getEnumLabelOids("telephone_mode", telephone_mode_labels, mode_oids, TMODE_MODE_COUNT);
    }
}

/*
static void init_typlen_text() {
    if (typlen_text_typalign != 0)
        return;

    get_typlenbyvalalign(TEXTOID, &typlen_text, &typlen_text_typbyval, &typlen_text_typalign);
}*/

static void init_typlen_telephone() {
    if (typlen_telephone_oid != 0)
        return;

    typlen_telephone_oid = TypenameGetTypid("telephone");
    get_typlenbyvalalign(typlen_telephone_oid, &typlen_telephone, &typlen_telephone_typbyval, &typlen_telephone_typalign);
}

static int enum_label_cmp(const void *left, const void *right) {
    const char *l = ((EnumLabel *) left)->label;
    const char *r = ((EnumLabel *) right)->label;
    return strcmp(l, r);
}

/*-------------------------------------------------------------------------
 * enum code
 * Copyright (c) 2010, PostgreSQL Global Development Group
 * Written by Joey Adams <joeyadams3.14159@gmail.com>.
 *-------------------------------------------------------------------------
 * getEnumLabelOids
 *    Look up the OIDs of enum labels.  Enum label OIDs are needed to
 *    return values of a custom enum type from a C function.
 *
 *    Callers should typically cache the OIDs produced by this function
 *    using fn_extra, as retrieving enum label OIDs is somewhat expensive.
 *
 *    Every labels[i].index must be between 0 and count, and oid_out
 *    must be allocated to hold count items.  Note that getEnumLabelOids
 *    sorts the labels[] array passed to it.
 *
 *    Any labels not found in the enum will have their corresponding
 *    oid_out entries set to InvalidOid.
 *
 *    Sample usage:
 *
 *    -- SQL --
 *    CREATE TYPE colors AS ENUM ('red', 'green', 'blue');
 *
 *    -- C --
 *    enum Colors {RED, GREEN, BLUE, COLOR_COUNT};
 *
 *    static EnumLabel enum_labels[COLOR_COUNT] =
 *    {
 *        {RED,   "red"},
 *        {GREEN, "green"},
 *        {BLUE,  "blue"}
 *    };
 *
 *    Oid *label_oids = palloc(COLOR_COUNT * sizeof(Oid));
 *    getEnumLabelOids("colors", enum_labels, label_oids, COLOR_COUNT);
 *
 *    PG_RETURN_OID(label_oids[GREEN]);
 */
void getEnumLabelOids(const char *typname, EnumLabel labels[], Oid oid_out[], int count) {
    CatCList   *list;
    Oid         enumtypoid;
    int         total;
    int         i;
    EnumLabel   key;
    EnumLabel  *found;

    enumtypoid = TypenameGetTypid(typname);
    Assert(OidIsValid(enumtypoid));

    qsort(labels, count, sizeof(EnumLabel), enum_label_cmp);

    for (i = 0; i < count; i++)
    {
        /* Initialize oid_out items to InvalidOid. */
        oid_out[i] = InvalidOid;

        /* Make sure EnumLabel indices are in range. */
        Assert(labels[i].index >= 0 && labels[i].index < count);
    }

    list = SearchSysCacheList1(ENUMTYPOIDNAME,
                               ObjectIdGetDatum(enumtypoid));
    total = list->n_members;

    for (i = 0; i < total; i++)
    {
        HeapTuple   tup = &list->members[i]->tuple;
        Oid         oid = HeapTupleGetOid(tup);
        Form_pg_enum en = (Form_pg_enum) GETSTRUCT(tup);

        key.label = NameStr(en->enumlabel);
        found = bsearch(&key, labels, count, sizeof(EnumLabel), enum_label_cmp);
        if (found != NULL)
            oid_out[found->index] = oid;
    }

    ReleaseCatCacheList(list);
}

static void set_parse_out(struct digit_letter digit_letter, struct parse_buffer * parse_out, int field) {
    int field_changed = 0;
    int pause_count;
    if (parse_out->digit_pos_next != 0) {
        // Only needed for the digits mode.
        if (digit_letter.value == DIGIT_SPECIAL) {
            if (WHITESPACE_IS_DATA) {
                if (parse_out->digit_values[parse_out->digit_pos_next - 1] == digit_letter.value) {
                    return;
                }
            } else {
                if ((parse_out->digit_valuesmask[parse_out->digit_pos_next - 1] & VALUEMASK_CONFIRM) == VALUEMASK_CONFIRM)
                    return;

                if (((parse_out->digit_valuesmask[parse_out->digit_pos_next - 1] >> VALUEBIT_PAUSE) & VALUEPARTMASK_PAUSE) > 0)
                    return;

                parse_out->digit_valuesmask[parse_out->digit_pos_next - 1] |= VALUEMASK_WHITESPACE;
                return;
            }
        } else if (digit_letter.value == DIGIT_CONFIRM) {
            if (CONFIRM_IS_DATA) {
                while (parse_out->digit_values[parse_out->digit_pos_next - 1] == DIGIT_PAUSE) {
                    parse_out->digit_pos_next--;
                }
                switch (parse_out->digit_values[parse_out->digit_pos_next - 1]) {
                    case DIGIT_SPECIAL:
                        parse_out->digit_pos_next--;
                    break;
                    case DIGIT_CONFIRM:
                    return;
                }
            } else {
                parse_out->digit_valuesmask[parse_out->digit_pos_next - 1] |= VALUEMASK_CONFIRM;
                // Clear whitespace and pause
                parse_out->digit_valuesmask[parse_out->digit_pos_next - 1] &= ~(1L << VALUEBIT_WHITESPACE);
                parse_out->digit_valuesmask[parse_out->digit_pos_next - 1] &= ~(VALUEPARTMASK_PAUSE << VALUEBIT_PAUSE);
                return;
            }
        } else if (digit_letter.value == DIGIT_PAUSE) {
            if (PAUSE_IS_DATA) {
                switch (parse_out->digit_values[parse_out->digit_pos_next - 1]) {
                    case DIGIT_SPECIAL:
                        parse_out->digit_pos_next--;
                    break;
                }
            } else {
                if ((parse_out->digit_valuesmask[parse_out->digit_pos_next - 1] & VALUEMASK_CONFIRM) == VALUEMASK_CONFIRM)
                    return;

                pause_count = (parse_out->digit_valuesmask[parse_out->digit_pos_next - 1] >> VALUEBIT_PAUSE) & VALUEPARTMASK_PAUSE;
                parse_out->digit_valuesmask[parse_out->digit_pos_next - 1] ^= pause_count << VALUEBIT_PAUSE; // Blank it out.
                if (pause_count < VALUEPARTMASK_PAUSE)
                    pause_count++;
                parse_out->digit_valuesmask[parse_out->digit_pos_next - 1] |= pause_count << VALUEBIT_PAUSE; // Merge it back in.
                // Clear whitespace
                parse_out->digit_valuesmask[parse_out->digit_pos_next - 1] &= ~(1L << VALUEBIT_WHITESPACE);
                return;
            }
        } else if (digit_letter.value == DIGIT_SLASH) {
            switch (parse_out->digit_values[parse_out->digit_pos_next - 1]) {
                case DIGIT_SLASH:
                    parse_out->digit_pos_next--;
                break;
            }
            // Clear whitespace
            parse_out->digit_valuesmask[parse_out->digit_pos_next - 1] &= ~(1L << VALUEBIT_WHITESPACE);
        }

        switch (parse_out->digit_values[parse_out->digit_pos_next - 1]) {
            case DIGIT_SLASH:
                // Clear whitespace
                parse_out->digit_valuesmask[parse_out->digit_pos_next - 1] &= ~(1L << VALUEBIT_WHITESPACE);
            break;
        }

        // Used for parsing.  Some parse stages require that field_pos be set to zero before this function is called.
        field_changed = parse_out->digit_fields[parse_out->digit_pos_next - 1] != field;
        if (field_changed) {
            parse_out->field_pos = 0;
        }
    }

    parse_out->digit_values[parse_out->digit_pos_next]      = digit_letter.value;
    parse_out->digit_valuesmask[parse_out->digit_pos_next]  = digit_letter.letter_index;
    parse_out->digit_fields[parse_out->digit_pos_next++]    = field;
    parse_out->field_pos++;
}

static struct digit_letter digit_letter_special() {
    struct digit_letter digit_letter;

    digit_letter.value = DIGIT_SPECIAL;
    digit_letter.letter_index = 0;
    digit_letter.invalid_char = 0;

    return digit_letter;
}

static void field_value_add(struct parse_buffer * parse_out) {
    parse_out->field_value *= 10; // Move the significant decimal (not binary) digit to the left.
    parse_out->field_value += parse_out->digit_values[parse_out->digit_pos_next - 1];
}

// Find last used code_id with:
// grep 'set_parse_error(parse_out, ERROR_' src/telephone.c | cut -d ',' -f 3 | cut -d ')' -f 1 | sed 's/ //' | sort -n | tail -n 1
// Use -1 if you have not assigned one yet.  Make sure to replace copied numbers with -1 after a copy and paste.
// Reassign duplicate IDs if any show up with this:
// grep 'set_parse_error(parse_out, ERROR_' src/telephone.c | cut -d ',' -f 3 | cut -d ')' -f 1 | sed 's/ //' | sort -n | uniq -d
static void set_parse_error(struct parse_buffer * parse_out, uint error_code, uint code_id) {
    parse_out->parse_state  = STATE_ERROR;
    parse_out->error_code   = error_code;
    parse_out->code_id      = code_id;
}

static void use_alt_state_uk(struct parse_buffer * parse_out) {
    switch (parse_out->alt_state) {
        case ALT_STATE_NULL:
            set_parse_error(parse_out, ERROR_AREA_CODE_NOT_FOUND, 79);
        break;
        case ALT_STATE_UK_4:
            parse_out->parse_state      = STATE_UK_SUB_1_6;
            parse_out->field_pos        = parse_out->digit_pos_next - 6;
            parse_out->service_type     = TSERV_LAND;
            parse_out->digit_fields[6]  = FIELD_UK_SUB1;
        break;
        default:
            ereport(ERROR, (
            errmsg("Alt state case missing.")));
    }
}

static void use_alt_state_japan(struct parse_buffer * parse_out) {
    switch (parse_out->alt_state) {
        case ALT_STATE_NULL:
            set_parse_error(parse_out, ERROR_AREA_CODE_NOT_FOUND, 117);
        break;
        case ALT_STATE_JAPAN_LAND_2:
            if (parse_out->digit_pos_next == 7) {
                parse_out->parse_state  = STATE_JAPAN_SUB_2_4;
                parse_out->field_pos    = 0;
            } else {
                parse_out->parse_state  = STATE_JAPAN_SUB_1_3_4;
                parse_out->field_pos    = parse_out->digit_pos_next - 4;
            }
            parse_out->service_type     = TSERV_LAND;
            parse_out->digit_fields[4]  = FIELD_JAPAN_SUB1;
            parse_out->digit_fields[5]  = FIELD_JAPAN_SUB1;
            parse_out->digit_fields[6]  = FIELD_JAPAN_SUB1;
        break;
        case ALT_STATE_JAPAN_LAND_3:
            if (parse_out->digit_pos_next == 7) {
                parse_out->parse_state  = STATE_JAPAN_SUB_2_4;
                parse_out->field_pos    = 0;
            } else {
                parse_out->parse_state  = STATE_JAPAN_SUB_1_2_4;
                parse_out->field_pos    = parse_out->digit_pos_next - 5;
            }
            parse_out->service_type     = TSERV_LAND;
            parse_out->digit_fields[5]  = FIELD_JAPAN_SUB1;
            parse_out->digit_fields[6]  = FIELD_JAPAN_SUB1;
        break;
        case ALT_STATE_JAPAN_LAND_4:
            if (parse_out->digit_pos_next == 7) {
                parse_out->parse_state  = STATE_JAPAN_SUB_2_4;
                parse_out->field_pos    = 0;
            } else {
                parse_out->parse_state  = STATE_JAPAN_SUB_1_1_4;
                parse_out->field_pos    = parse_out->digit_pos_next - 6;
            }
            parse_out->service_type     = TSERV_LAND;
            parse_out->digit_fields[6]  = FIELD_JAPAN_SUB1;
        break;
        default:
            ereport(ERROR, (
            errmsg("Alt state case missing.")));
    }
}

static void parse_digit(struct digit_letter digit_letter, struct parse_buffer * parse_out) {
    char digit_value = digit_letter.value;
    int parse_state = parse_out->parse_state;
    parse_out->parse_state = STATE_ERROR;

    if (digit_letter.invalid_char != 0) {
        // parse_out->field_pos must be set to 0 when parse_state is changed to STATE_EXTENSION_START.
        if (parse_state == STATE_EXTENSION_START) {
            switch (parse_out->field_pos) {
                case 0:
                    switch (digit_letter.invalid_char) {
                        case 'e': // Ext, Extn
                        case 'x': // X
                            parse_out->field_pos++;
                            parse_out->parse_state = STATE_EXTENSION_START;
                            return;
                    }
                break;
                case 1:
                    switch (digit_letter.invalid_char) {
                        case 'x': // eXt, eXtn
                            parse_out->field_pos++;
                            parse_out->parse_state = STATE_EXTENSION_START;
                            return;
                    }
                break;
                case 2:
                    switch (digit_letter.invalid_char) {
                        case 't': // exT, exTn
                            parse_out->field_pos++;
                            parse_out->parse_state = STATE_EXTENSION_START;
                            return;
                    }
                break;
                case 3:
                    switch (digit_letter.invalid_char) {
                        case 'n': // extN
                            parse_out->field_pos++;
                            parse_out->parse_state = STATE_EXTENSION_START;
                            return;
                    }
                break;
            }
        }

        set_parse_error(parse_out, ERROR_INVALID_CHAR, 1);
        return;
    }

    switch (parse_state) {
        case STATE_START:
            switch (digit_value) {
                case DIGIT_PLUS:
                    parse_out->parse_state = STATE_CALLING_CODE_START;
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                case DIGIT_STAR:
                case DIGIT_POUND:
                    set_parse_out(digit_letter_special(), parse_out, FIELD_DIGITS_QUALIFIER);
                    set_parse_out(digit_letter, parse_out, FIELD_DIGITS);
                    parse_out->parse_state = STATE_DIGITS;
                break;
                case DIGIT_SPECIAL:
                case DIGIT_PAUSE:
                case DIGIT_CONFIRM:
                case DIGIT_SLASH:
                    parse_out->parse_state = STATE_START;
                break;
             }
        break;
        case STATE_CALLING_CODE_START:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    parse_out->parse_state = STATE_CALLING_CODE_START;
                break;
                case 1:
                    set_parse_out(digit_letter, parse_out, FIELD_CALLING_CODE);
                    parse_out->parse_state = STATE_NANP_NPA_START;
                break;
                case 2:
                    set_parse_out(digit_letter, parse_out, FIELD_CALLING_CODE);
                    parse_out->parse_state = STATE_CALLING_CODE_ZONE2;
                break;
                case 3:
                    set_parse_out(digit_letter, parse_out, FIELD_CALLING_CODE);
                    parse_out->parse_state = STATE_CALLING_CODE_ZONE3;
                break;
                case 4:
                    set_parse_out(digit_letter, parse_out, FIELD_CALLING_CODE);
                    parse_out->parse_state = STATE_CALLING_CODE_ZONE4;
                break;
                case 5:
                    set_parse_out(digit_letter, parse_out, FIELD_CALLING_CODE);
                    parse_out->parse_state = STATE_CALLING_CODE_ZONE5;
                break;
                case 6:
                    set_parse_out(digit_letter, parse_out, FIELD_CALLING_CODE);
                    parse_out->parse_state = STATE_CALLING_CODE_ZONE6;
                break;
                case 7:
                    set_parse_out(digit_letter, parse_out, FIELD_CALLING_CODE);
                    parse_out->parse_state = STATE_RUSSIA_AREA_CODE_START;
                break;
                case 8:
                    set_parse_out(digit_letter, parse_out, FIELD_CALLING_CODE);
                    parse_out->parse_state = STATE_CALLING_CODE_ZONE8;
                break;
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_CALLING_CODE);
                    parse_out->parse_state = STATE_CALLING_CODE_ZONE9;
                break;
                default:
                    set_parse_error(parse_out, ERROR_CALLING_CODE_START_INVALID, 2);
                break;
            }
        break;
        case STATE_NANP_NPA_START:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    parse_out->parse_state = STATE_NANP_NPA_START;
                break;
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_NANP_NPAC);
                    parse_out->parse_state = STATE_NANP_NPA_DIGITS;
                    parse_out->field_value = digit_value;
                break;
                default:
                    set_parse_error(parse_out, ERROR_AREA_CODE_START_INVALID, 3);
                break;
            }
        break;
        case STATE_NANP_NPA_DIGITS:
            switch (digit_value) {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_NANP_NPAC);
                    field_value_add(parse_out);
                    if (parse_out->field_pos < 3) {
                        parse_out->parse_state = STATE_NANP_NPA_DIGITS;
                    } else {
                        switch (parse_out->field_value) {
                            case 201: // NJ
                            case 202: // DC
                            case 203: // CT
                            case 204: // MANITOBA
                            case 205: // AL
                            case 206: // WA
                            case 207: // ME
                            case 208: // ID
                            case 209: // CA
                            case 210: // TX
                            case 212: // NY
                            case 213: // CA
                            case 214: // TX
                            case 215: // PA
                            case 216: // OH
                            case 217: // IL
                            case 218: // MN
                            case 219: // IN
                            case 220: // OH
                            case 224: // IL
                            case 225: // LA
                            case 226: // ONTARIO
                            case 228: // MS
                            case 229: // GA
                            case 231: // MI
                            case 234: // OH
                            case 236: // BRITISH COLUMBIA
                            case 239: // FL
                            case 240: // MD
                            case 242: // BAHAMAS
                            case 246: // BARBADOS
                            case 248: // MI
                            case 249: // ONTARIO
                            case 250: // BRITISH COLUMBIA
                            case 251: // AL
                            case 252: // NC
                            case 253: // WA
                            case 254: // TX
                            case 256: // AL
                            case 260: // IN
                            case 262: // WI
                            case 264: // ANGUILLA
                            case 267: // PA
                            case 268: // ANTIGUA/BARBUDA
                            case 269: // MI
                            case 270: // KY
                            case 272: // PA
                            case 276: // VA
                            case 281: // TX
                            case 284: // BRITISH VIRGIN ISLANDS
                            case 289: // ONTARIO
                            case 301: // MD
                            case 302: // DE
                            case 303: // CO
                            case 304: // WV
                            case 305: // FL
                            case 306: // SASKATCHEWAN
                            case 307: // WY
                            case 308: // NE
                            case 309: // IL
                            case 310: // CA
                            case 312: // IL
                            case 313: // MI
                            case 314: // MO
                            case 315: // NY
                            case 316: // KS
                            case 317: // IN
                            case 318: // LA
                            case 319: // IA
                            case 320: // MN
                            case 321: // FL
                            case 323: // CA
                            case 325: // TX
                            case 330: // OH
                            case 331: // IL
                            case 334: // AL
                            case 336: // NC
                            case 337: // LA
                            case 339: // MA
                            case 340: // USVI
                            case 343: // ONTARIO
                            case 345: // CAYMAN ISLANDS
                            case 346: // TX
                            case 347: // NY
                            case 351: // MA
                            case 352: // FL
                            case 360: // WA
                            case 361: // TX
                            case 364: // KY
                            case 365: // ONTARIO
                            case 385: // UT
                            case 386: // FL
                            case 401: // RI
                            case 402: // NE
                            case 403: // ALBERTA
                            case 404: // GA
                            case 405: // OK
                            case 406: // MT
                            case 407: // FL
                            case 408: // CA
                            case 409: // TX
                            case 410: // MD
                            case 412: // PA
                            case 413: // MA
                            case 414: // WI
                            case 415: // CA
                            case 416: // ONTARIO
                            case 417: // MO
                            case 418: // QUEBEC
                            case 419: // OH
                            case 423: // TN
                            case 424: // CA
                            case 425: // WA
                            case 430: // TX
                            case 431: // MANITOBA
                            case 432: // TX
                            case 434: // VA
                            case 435: // UT
                            case 437: // ONTARIO
                            case 438: // QUEBEC
                            case 440: // OH
                            case 441: // BERMUDA
                            case 442: // CA
                            case 443: // MD
                            case 450: // QUEBEC
                            case 458: // OR
                            case 469: // TX
                            case 470: // GA
                            case 473: // GRENADA
                            case 475: // CT
                            case 478: // GA
                            case 479: // AR
                            case 480: // AZ
                            case 484: // PA
                            case 501: // AR
                            case 502: // KY
                            case 503: // OR
                            case 504: // LA
                            case 505: // NM
                            case 506: // NEW BRUNSWICK
                            case 507: // MN
                            case 508: // MA
                            case 509: // WA
                            case 510: // CA
                            case 512: // TX
                            case 513: // OH
                            case 514: // QUEBEC
                            case 515: // IA
                            case 516: // NY
                            case 517: // MI
                            case 518: // NY
                            case 519: // ONTARIO
                            case 520: // AZ
                            case 530: // CA
                            case 531: // NE
                            case 534: // WI
                            case 539: // OK
                            case 540: // VA
                            case 541: // OR
                            case 551: // NJ
                            case 559: // CA
                            case 561: // FL
                            case 562: // CA
                            case 563: // IA
                            case 567: // OH
                            case 570: // PA
                            case 571: // VA
                            case 573: // MO
                            case 574: // IN
                            case 575: // NM
                            case 579: // QUEBEC
                            case 580: // OK
                            case 581: // QUEBEC
                            case 585: // NY
                            case 586: // MI
                            case 587: // ALBERTA
                            case 601: // MS
                            case 602: // AZ
                            case 603: // NH
                            case 604: // BRITISH COLUMBIA
                            case 605: // SD
                            case 606: // KY
                            case 607: // NY
                            case 608: // WI
                            case 609: // NJ
                            case 610: // PA
                            case 612: // MN
                            case 613: // ONTARIO
                            case 614: // OH
                            case 615: // TN
                            case 616: // MI
                            case 617: // MA
                            case 618: // IL
                            case 619: // CA
                            case 620: // KS
                            case 623: // AZ
                            case 626: // CA
                            case 628: // CA
                            case 629: // TN
                            case 630: // IL
                            case 631: // NY
                            case 636: // MO
                            case 639: // SASKATCHEWAN
                            case 641: // IA
                            case 646: // NY
                            case 647: // ONTARIO
                            case 649: // TURKS & CAICOS ISLANDS
                            case 650: // CA
                            case 651: // MN
                            case 657: // CA
                            case 660: // MO
                            case 661: // CA
                            case 662: // MS
                            case 664: // MONTSERRAT
                            case 667: // MD
                            case 669: // CA
                            case 670: // CNMI
                            case 671: // GU
                            case 678: // GA
                            case 681: // WV
                            case 682: // TX
                            case 684: // AS
                            case 701: // ND
                            case 702: // NV
                            case 703: // VA
                            case 704: // NC
                            case 705: // ONTARIO
                            case 706: // GA
                            case 707: // CA
                            case 708: // IL
                            case 709: // NEWFOUNDLAND
                            case 712: // IA
                            case 713: // TX
                            case 714: // CA
                            case 715: // WI
                            case 716: // NY
                            case 717: // PA
                            case 718: // NY
                            case 719: // CO
                            case 720: // CO
                            case 721: // SINT MAARTEN
                            case 724: // PA
                            case 725: // NV
                            case 727: // FL
                            case 731: // TN
                            case 732: // NJ
                            case 734: // MI
                            case 737: // TX
                            case 740: // OH
                            case 747: // CA
                            case 754: // FL
                            case 757: // VA
                            case 758: // ST. LUCIA
                            case 760: // CA
                            case 762: // GA
                            case 763: // MN
                            case 765: // IN
                            case 767: // DOMINICA
                            case 769: // MS
                            case 770: // GA
                            case 772: // FL
                            case 773: // IL
                            case 774: // MA
                            case 775: // NV
                            case 778: // BRITISH COLUMBIA
                            case 779: // IL
                            case 780: // ALBERTA
                            case 781: // MA
                            case 782: // NOVA SCOTIA - PRINCE EDWARD ISLAND
                            case 784: // ST. VINCENT & GRENADINES
                            case 785: // KS
                            case 786: // FL
                            case 787: // PUERTO RICO
                            case 801: // UT
                            case 802: // VT
                            case 803: // SC
                            case 804: // VA
                            case 805: // CA
                            case 806: // TX
                            case 807: // ONTARIO
                            case 808: // HI
                            case 809: // DOMINICAN REPUBLIC
                            case 810: // MI
                            case 812: // IN
                            case 813: // FL
                            case 814: // PA
                            case 815: // IL
                            case 816: // MO
                            case 817: // TX
                            case 818: // CA
                            case 819: // QUEBEC
                            case 828: // NC
                            case 829: // DOMINICAN REPUBLIC
                            case 830: // TX
                            case 831: // CA
                            case 832: // TX
                            case 843: // SC
                            case 845: // NY
                            case 847: // IL
                            case 848: // NJ
                            case 849: // DOMINICAN REPUBLIC
                            case 850: // FL
                            case 854: // SC
                            case 856: // NJ
                            case 857: // MA
                            case 858: // CA
                            case 859: // KY
                            case 860: // CT
                            case 862: // NJ
                            case 863: // FL
                            case 864: // SC
                            case 865: // TN
                            case 867: // YUKON-NW TERR. - NUNAVUT
                            case 868: // TRINIDAD & TOBAGO
                            case 869: // ST. KITTS & NEVIS
                            case 870: // AR
                            case 872: // IL
                            case 873: // QUEBEC
                            case 876: // JAMAICA
                            case 878: // PA
                            case 901: // TN
                            case 902: // NOVA SCOTIA - PRINCE EDWARD ISLAND
                            case 903: // TX
                            case 904: // FL
                            case 905: // ONTARIO
                            case 906: // MI
                            case 907: // AK
                            case 908: // NJ
                            case 909: // CA
                            case 910: // NC
                            case 912: // GA
                            case 913: // KS
                            case 914: // NY
                            case 915: // TX
                            case 916: // CA
                            case 917: // NY
                            case 918: // OK
                            case 919: // NC
                            case 920: // WI
                            case 925: // CA
                            case 928: // AZ
                            case 929: // NY
                            case 930: // IN
                            case 931: // TN
                            case 936: // TX
                            case 937: // OH
                            case 938: // AL
                            case 939: // PUERTO RICO
                            case 940: // TX
                            case 941: // FL
                            case 947: // MI
                            case 949: // CA
                            case 951: // CA
                            case 952: // MN
                            case 954: // FL
                            case 956: // TX
                            case 959: // CT
                            case 970: // CO
                            case 971: // OR
                            case 972: // TX
                            case 973: // NJ
                            case 978: // MA
                            case 979: // TX
                            case 980: // NC
                            case 984: // NC
                            case 985: // LA
                            case 989: // MI
                                parse_out->parse_state = STATE_NANP_COC_START;
                                parse_out->service_type = TSERV_LAND_OR_CELL;
                            break;
                            case 227: // MD
                            case 274: // WI
                            case 283: // OH
                            case 327: // AR
                            case 332: // NY
                            case 380: // OH
                            case 447: // IL
                            case 463: // IN
                            case 464: // IL
                            case 548: // ONTARIO
                            case 557: // MO
                            case 564: // WA
                            case 659: // AL
                            case 679: // MI
                            case 680: // NY
                            case 689: // FL
                            case 730: // IL
                            case 743: // NC
                            case 825: // ALBERTA
                            case 934: // NY
                            case 975: // MO
                            case 986: // ID
                                // Planned, but not yet in service
                                parse_out->parse_state = STATE_NANP_COC_START;
                                parse_out->service_type = TSERV_LAND_OR_CELL;
                            break;
                            case 456: // Inbound International
                                parse_out->parse_state = STATE_NANP_COC_START;
                                parse_out->service_type = TSERV_CARRIER_SERVICES_INTERNATIONAL;
                            break;
                            case 600: // Canadian Services
                            case 622: // Canadian Non-Geographic Services
                                parse_out->parse_state = STATE_NANP_COC_START;
                                parse_out->service_type = TSERV_OTHER;
                            break;
                            case 700: // Interexchange Carrier Services
                                parse_out->parse_state = STATE_NANP_COC_START;
                                parse_out->service_type = TSERV_CARRIER_SERVICES_INTRA;
                            break;
                            case 710: // US Government
                                parse_out->parse_state = STATE_NANP_COC_START;
                                parse_out->service_type = TSERV_GOVERNMENT;
                            break;
                            case 500: // Non-Geographic Services
                            case 533: // Non-Geographic Services
                            case 544: // Non-Geographic Services
                            case 566: // Non-Geographic Services
                            case 577: // Non-Geographic Services
                            case 588: // Personal Communication Service
                                parse_out->parse_state = STATE_NANP_COC_START;
                                parse_out->service_type = TSERV_FOLLOW_ME;
                            break;
                            case 800: // Toll-Free
                            case 844: // Toll-Free
                            case 855: // Toll-Free
                            case 866: // Toll-Free
                            case 877: // Toll-Free
                            case 888: // Toll-Free
                                parse_out->parse_state = STATE_NANP_COC_START;
                                parse_out->service_type = TSERV_TOLLFREE;
                            break;
                            case 900: // Premium Services
                                parse_out->parse_state = STATE_NANP_COC_START;
                                parse_out->service_type = TSERV_CHARGE;
                            break;
                            default:
                                set_parse_error(parse_out, ERROR_AREA_CODE_NOT_FOUND, 4);
                            break;
                        }
                        //parse_out->service_type = TSERV_QUERY_NANP;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_AREA_CODE_DIGITS_INVALID, 5);
                break;
            }
        break;
        case STATE_NANP_COC_START:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    parse_out->parse_state = STATE_NANP_COC_START;
                break;
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_NANP_COC);
                    parse_out->parse_state = STATE_NANP_COC_DIGITS;
                break;
                default:
                    set_parse_error(parse_out, ERROR_EXCHANGE_CODE_START_INVALID, 6);
                break;
            }
        break;
        case STATE_NANP_COC_DIGITS:
            switch (digit_value) {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_NANP_COC);
                    if (parse_out->field_pos < 3) {
                        parse_out->parse_state = STATE_NANP_COC_DIGITS;
                    } else {
                        parse_out->parse_state = STATE_NANP_SUB_START;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_EXCHANGE_CODE_DIGITS_INVALID, 7);
                break;
            }
        break;
        case STATE_NANP_SUB_START:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    parse_out->parse_state = STATE_NANP_SUB_START;
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_NANP_SUB);
                    parse_out->parse_state = STATE_NANP_SUB_DIGITS;
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_START_INVALID, 8);
                break;
            }
        break;
        case STATE_NANP_SUB_DIGITS:
            switch (digit_value) {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_NANP_SUB);
                    if (parse_out->field_pos < 4) {
                        parse_out->parse_state = STATE_NANP_SUB_DIGITS;
                    } else {
                        if (
                            parse_out->digit_values[ 1] == 8 &&
                            parse_out->digit_values[ 2] == 0 &&
                            parse_out->digit_values[ 3] == 5 &&
                            parse_out->digit_values[ 4] == 6 &&
                            parse_out->digit_values[ 5] == 3 &&
                            parse_out->digit_values[ 6] == 7 &&
                            parse_out->digit_values[ 7] == 7 &&
                            parse_out->digit_values[ 8] == 2 &&
                            parse_out->digit_values[ 9] == 4 &&
                            parse_out->digit_values[10] == 3) {
                            parse_out->service_type = TSERV_VOICEMAIL;
                        }
                        parse_out->parse_state = STATE_EXTENSION_START;
                        parse_out->field_pos = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 9);
                break;
            }
        break;
        case STATE_EXTENSION_START:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                case DIGIT_PAUSE:
                case DIGIT_CONFIRM:
                    parse_out->parse_state = STATE_EXTENSION_START;
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                case DIGIT_STAR:
                case DIGIT_POUND:
                    set_parse_out(digit_letter, parse_out, FIELD_EXTENSION);
                    parse_out->parse_state = STATE_EXTENSION_DIGITS;
                break;
                default:
                    set_parse_error(parse_out, ERROR_EXTENSION_START_INVALID, 10);
                break;
            }
        break;
        case STATE_EXTENSION_DIGITS:
            switch (digit_value) {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                case DIGIT_STAR:
                case DIGIT_POUND:
                case DIGIT_PAUSE:
                case DIGIT_CONFIRM:
                case DIGIT_SPECIAL:
                case DIGIT_SLASH:
                    set_parse_out(digit_letter, parse_out, FIELD_EXTENSION);
                    parse_out->parse_state = STATE_EXTENSION_DIGITS;
                break;
                default:
                    set_parse_error(parse_out, ERROR_EXTENSION_DIGITS_INVALID, 11);
                break;
            }
        break;
        case STATE_DIGITS:
            switch (digit_value) {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                case DIGIT_STAR:
                case DIGIT_POUND:
                case DIGIT_PAUSE:
                case DIGIT_CONFIRM:
                case DIGIT_SPECIAL:
                case DIGIT_SLASH:
                    set_parse_out(digit_letter, parse_out, FIELD_DIGITS);
                    parse_out->parse_state = STATE_DIGITS;
                break;
                default:
                    set_parse_error(parse_out, ERROR_DIGITS_INVALID, 12);
                break;
             }
        break;
        case STATE_DIGITS_IGNORE_QUALIFIER:
            parse_out->parse_state      = STATE_DIGITS;
        break;
        case STATE_CALLING_CODE_ZONE2: // mostly Africa
            switch (digit_value) {
                case 0: // Egypt
                case 7: // South Africa
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 8:
                case 9:
                    set_parse_error(parse_out, ERROR_CALLING_CODE_NEED_CODE, 13);
                break;
                default:
                    set_parse_error(parse_out, ERROR_CALLING_CODE_DIGITS_INVALID, 14);
                break;
            }
        break;
        case STATE_CALLING_CODE_ZONE3: // Europe
            switch (digit_value) {
                case 3: // France
                    set_parse_out(digit_letter, parse_out, FIELD_CALLING_CODE);
                    parse_out->parse_state = STATE_FRANCE_START;
                break;
                case 9: // Italy
                    set_parse_out(digit_letter, parse_out, FIELD_CALLING_CODE);
                    parse_out->parse_state = STATE_ITALY_AREA_START;
                break;
                case 0: // Greece
                case 1: // Netherlands
                case 2: // Belgium
                case 4: // Spain
                case 6: // Hungary
                    set_parse_error(parse_out, ERROR_CALLING_CODE_NEED_CODE, 15);
                break;
                case 5:
                    set_parse_out(digit_letter, parse_out, FIELD_CALLING_CODE);
                    parse_out->parse_state = STATE_CALLING_CODE_35;
                break;
                case 7:
                case 8:
                    set_parse_error(parse_out, ERROR_CALLING_CODE_NEED_CODE, 16);
                break;
                default:
                    set_parse_error(parse_out, ERROR_CALLING_CODE_DIGITS_INVALID, 17);
                break;
            }
        break;
        case STATE_FRANCE_START:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    parse_out->parse_state = STATE_FRANCE_START;
                break;
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                    set_parse_out(digit_letter, parse_out, FIELD_FRANCE_AREA_CODE);
                    parse_out->parse_state  = STATE_FRANCE_DIGITS;
                    parse_out->service_type = TSERV_LAND;
                    parse_out->field_pos    = 0;
                break;
                case 6:
                case 7:
                    set_parse_out(digit_letter, parse_out, FIELD_FRANCE_AREA_CODE);
                    parse_out->parse_state  = STATE_FRANCE_DIGITS;
                    parse_out->service_type = TSERV_CELL;
                    parse_out->field_pos    = 0;
                break;
                case 8:
                    set_parse_out(digit_letter, parse_out, FIELD_FRANCE_AREA_CODE);
                    parse_out->parse_state  = STATE_FRANCE_DIGITS;
                    parse_out->field_value  = digit_value;
                    parse_out->field_pos    = 0;
                break;
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_FRANCE_AREA_CODE);
                    parse_out->parse_state  = STATE_FRANCE_DIGITS;
                    parse_out->service_type = TSERV_VOIP;
                    parse_out->field_pos    = 0;
                break;
                default:
                    set_parse_error(parse_out, ERROR_AREA_CODE_START_INVALID, 118);
                break;
            }
        break;
        case STATE_FRANCE_DIGITS:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos % 2 == 0)
                        parse_out->parse_state = STATE_FRANCE_DIGITS;
                    else
                        set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 119);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    if (parse_out->service_type == TSERV_NEED_TO_PARSE) {
                        if (parse_out->field_value == 8) {
                            if (digit_value == 0)
                                parse_out->service_type = TSERV_TOLLFREE;
                            else
                                parse_out->service_type = TSERV_CHARGE;
                        } else {
                            set_parse_error(parse_out, ERROR_INVALID_STATE, 120);
                        }
                    }
                    set_parse_out(digit_letter, parse_out, FIELD_FRANCE_SUB);
                    if (parse_out->field_pos < 8) {
                        parse_out->parse_state = STATE_FRANCE_DIGITS;
                    } else {
                        parse_out->parse_state  = STATE_EXTENSION_START;
                        parse_out->field_pos    = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 121);
                break;
            }
        break;
        case STATE_CALLING_CODE_35:
            switch (digit_value) {
                case 8: // Finland
                    set_parse_out(digit_letter, parse_out, FIELD_CALLING_CODE);
                    parse_out->parse_state = STATE_FINLAND_AREA_START;
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 9:
                    set_parse_error(parse_out, ERROR_CALLING_CODE_NEED_CODE, 18);
                break;
                default:
                    set_parse_error(parse_out, ERROR_CALLING_CODE_DIGITS_INVALID, 19);
                break;
            }
        break;
        case STATE_FINLAND_AREA_START:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    parse_out->parse_state = STATE_FINLAND_AREA_START;
                break;
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_FINLAND_AREA_CODE);
                    parse_out->parse_state = STATE_FINLAND_AREA_DIGITS;
                    parse_out->field_value = digit_value;
                break;
                default:
                    set_parse_error(parse_out, ERROR_AREA_CODE_START_INVALID, 20);
                break;
            }
        break;
        case STATE_FINLAND_AREA_DIGITS:
            switch (digit_value) {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_FINLAND_AREA_CODE);
                    parse_out->parse_state = STATE_FINLAND_AREA_DIGITS;
                    field_value_add(parse_out);
                    switch (parse_out->field_value) {
                        case 40:
                        case 41:
                        case 42:
                        case 43:
                        case 44:
                        case 45:
                        case 46:
                        case 49:
                        case 50:
                            parse_out->parse_state      = STATE_FINLAND_SUB_1_3_2_2;
                            parse_out->field_pos        = 0;
                            parse_out->service_type     = TSERV_CELL;
                        break;
                        case 800:
                            parse_out->parse_state      = STATE_FINLAND_SUB_1_3_3;
                            parse_out->field_pos        = 0;
                            parse_out->service_type     = TSERV_TOLLFREE;
                        break;
                        default:
                            if (parse_out->field_pos == 3)
                                set_parse_error(parse_out, ERROR_AREA_CODE_NOT_FOUND, 21);
                        break;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_AREA_CODE_DIGITS_INVALID, 22);
                break;
            }
        break;
        case STATE_FINLAND_SUB_1_3_2_2:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0)
                        parse_out->parse_state = STATE_FINLAND_SUB_1_3_2_2;
                    else
                        set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 23);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_FINLAND_SUB1);
                    if (parse_out->field_pos < 3) {
                        parse_out->parse_state = STATE_FINLAND_SUB_1_3_2_2;
                    } else {
                        parse_out->parse_state = STATE_FINLAND_SUB_2_2_2;
                        parse_out->field_pos = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 24);
                break;
            }
        break;
        case STATE_FINLAND_SUB_2_2_2:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0)
                        parse_out->parse_state = STATE_FINLAND_SUB_2_2_2;
                    else
                        set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 25);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_FINLAND_SUB2);
                    if (parse_out->field_pos < 2) {
                        parse_out->parse_state = STATE_FINLAND_SUB_2_2_2;
                    } else {
                        parse_out->parse_state = STATE_FINLAND_SUB_3_2;
                        parse_out->field_pos = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 26);
                break;
            }
        break;
        case STATE_FINLAND_SUB_3_2:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0)
                        parse_out->parse_state = STATE_FINLAND_SUB_3_2;
                    else
                        set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 27);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 28);
                break;
            }
        break;
        case STATE_FINLAND_SUB_1_3_3:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0)
                        parse_out->parse_state = STATE_FINLAND_SUB_1_3_3;
                    else
                        set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 29);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_FINLAND_SUB1);
                    if (parse_out->field_pos < 3) {
                        parse_out->parse_state = STATE_FINLAND_SUB_1_3_3;
                    } else {
                        parse_out->parse_state = STATE_FINLAND_SUB_2_3;
                        parse_out->field_pos = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 30);
                break;
            }
        break;
        case STATE_FINLAND_SUB_2_3:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0)
                        parse_out->parse_state = STATE_FINLAND_SUB_3_2;
                    else
                        set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 31);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_FINLAND_SUB2);
                    if (parse_out->field_pos < 3) {
                        parse_out->parse_state = STATE_FINLAND_SUB_2_3;
                    } else {
                        parse_out->parse_state  = STATE_EXTENSION_START;
                        parse_out->field_pos    = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 32);
                break;
            }
        break;
        case STATE_ITALY_AREA_START:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    parse_out->parse_state = STATE_ITALY_AREA_START;
                break;
                case 0:
                case 8:
                    set_parse_out(digit_letter, parse_out, FIELD_ITALY_AREA_CODE);
                    parse_out->parse_state = STATE_ITALY_AREA_DIGIT2;
                    parse_out->field_value = digit_value;
                break;
                case 3:
                    set_parse_out(digit_letter, parse_out, FIELD_ITALY_AREA_CODE);
                    parse_out->parse_state  = STATE_ITALY_CELL_PREFIX;
                    parse_out->field_pos    = 1;
                break;
                default:
                    set_parse_error(parse_out, ERROR_AREA_CODE_START_INVALID, 122);
                break;
            }
        break;
        case STATE_ITALY_AREA_DIGIT2:
            switch (digit_value) {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_ITALY_AREA_CODE);
                    field_value_add(parse_out);
                    switch (parse_out->field_value) {
                        case 2: // Milano
                        case 6: // Roma
                            parse_out->parse_state = STATE_ITALY_SUB_1_8;
                            parse_out->field_pos = 0;
                            parse_out->service_type = TSERV_LAND;
                        break;
                        default:
                            parse_out->parse_state = STATE_ITALY_AREA_DIGIT3;
                        break;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_AREA_CODE_DIGITS_INVALID, 123);
                break;
            }
        break;
        case STATE_ITALY_AREA_DIGIT3:
            switch (digit_value) {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_ITALY_AREA_CODE);
                    field_value_add(parse_out);
                    switch (parse_out->field_value) {
                        case 10: // Genova
                        case 11: // Torino
                        case 15: // Biella
                        case 19: // Savona
                        case 30: // Brescia
                        case 31: // Como
                        case 35: // Bergamo
                        case 39: // Monza
                        case 40: // Trieste
                        case 41: // Venezia (Mestre)
                        case 45: // Verona
                        case 49: // Padova
                        case 50: // Pisa
                        case 51: // Bologna
                        case 55: // Firenze
                        case 59: // Modena
                        case 70: // Cagliari
                        case 71: // Ancona
                        case 75: // Perugia
                        case 79: // Sassari
                        case 80: // Bari
                        case 81: // Napoli
                        case 85: // Pescara
                        case 89: // Salerno
                        case 90: // Messina
                        case 91: // Palermo
                        case 95: // Catania
                        case 99: // Taranto
                            parse_out->parse_state = STATE_ITALY_SUB_1_7;
                            parse_out->field_pos = 0;
                            parse_out->service_type = TSERV_LAND;
                        break;
                        case 800:
                        case 803:
                            parse_out->parse_state = STATE_ITALY_SUB_2_3_3;
                            parse_out->field_pos = 0;
                            parse_out->service_type = TSERV_TOLLFREE;
                        break;
                        default:
                            parse_out->parse_state = STATE_ITALY_AREA_DIGIT4;
                        break;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_AREA_CODE_DIGITS_INVALID, 124);
                break;
            }
        break;
        case STATE_ITALY_AREA_DIGIT4:
            switch (digit_value) {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_ITALY_AREA_CODE);
                    field_value_add(parse_out);
                    switch (parse_out->field_value) {
                        case 121: // Pinerolo
                        case 122: // Susa
                        case 123: // Lanzo Torinese
                        case 124: // Rivarolo Canavese
                        case 125: // Ivrea
                        case 131: // Alessandria
                        case 141: // Asti
                        case 142: // Casale Monferrato
                        case 143: // Novi Ligure
                        case 144: // Acqui Terme
                        case 161: // Vercelli
                        case 163: // Borgosesia
                        case 165: // Aosta
                        case 166: // St. Vincent
                        case 171: // Cuneo
                        case 172: // Savigliano
                        case 173: // Alba
                        case 174: // Mondovi'
                        case 175: // Saluzzo
                        case 182: // Albenga
                        case 183: // Imperia
                        case 184: // San remo
                        case 185: // Rapallo
                        case 187: // La Spezia
                        case 321: // Novara
                        case 322: // Arona
                        case 323: // Baveno
                        case 324: // Domodossola
                        case 331: // Busto Arsizio
                        case 332: // Varese
                        case 341: // Lecco
                        case 342: // Sondrio
                        case 343: // Chiavenna
                        case 344: // Menaggio
                        case 345: // S. Pellegrino Terme
                        case 346: // Clusone
                        case 362: // Seregno
                        case 363: // Treviglio
                        case 364: // Breno
                        case 365: // Salò
                        case 369: // Milano for mass call, until 31st December 2014
                        case 371: // Lodi
                        case 372: // Cremona
                        case 373: // Crema
                        case 374: // Soresina
                        case 375: // Casalmaggiore
                        case 376: // Mantova
                        case 377: // Codogno
                        case 381: // Vigevano
                        case 382: // Pavia
                        case 383: // Voghera
                        case 384: // Mortara
                        case 385: // Stradella
                        case 386: // Ostiglia
                        case 421: // S. Dona' di Piave
                        case 422: // Treviso
                        case 423: // Montebelluna
                        case 424: // Bassano del Grappa
                        case 425: // Rovigo
                        case 426: // Adria
                        case 427: // Spilimbergo
                        case 428: // Tarvisio
                        case 429: // Este
                        case 431: // Cervignano del Friuli
                        case 432: // Udine
                        case 433: // Tolmezzo
                        case 434: // Pordenone
                        case 435: // Pieve di Cadore
                        case 436: // Cortina d'Ampezzo
                        case 437: // Belluno
                        case 438: // Conegliano
                        case 439: // Feltre
                        case 442: // Legnago
                        case 444: // Vicenza
                        case 445: // Schio
                        case 461: // Trento
                        case 462: // Cavalese
                        case 463: // Cles
                        case 464: // Rovereto
                        case 465: // Tione di Trento
                        case 471: // Bolzano
                        case 472: // Bressanone
                        case 473: // Merano
                        case 474: // Brunico
                        case 481: // Gorizia
                        case 521: // Parma
                        case 522: // Reggio nell'Emilia
                        case 523: // Piacenza
                        case 524: // Fidenza
                        case 525: // Fornovo di Taro
                        case 532: // Ferrara
                        case 533: // Comacchio
                        case 534: // Porretta Terme
                        case 535: // Mirandola
                        case 536: // Sassuolo
                        case 541: // Rimini
                        case 542: // Imola
                        case 543: // Forlì
                        case 544: // Ravenna
                        case 545: // Lugo
                        case 546: // Faenza
                        case 547: // Cesena
                        case 549: // S. Marino (Rep. di)
                        case 564: // Grosseto
                        case 565: // Piombino
                        case 566: // Follonica
                        case 571: // Empoli
                        case 572: // Montecatini Terme
                        case 573: // Pistoia
                        case 574: // Prato
                        case 575: // Arezzo
                        case 577: // Siena
                        case 578: // Chianciano Terme
                        case 583: // Lucca
                        case 584: // Viareggio
                        case 585: // Massa
                        case 586: // Livorno
                        case 587: // Pontedera
                        case 588: // Volterra
                        case 721: // Pesaro
                        case 722: // Urbino
                        case 731: // Jesi
                        case 732: // Fabriano
                        case 733: // Macerata
                        case 734: // Fermo
                        case 735: // S. Benedetto del Tronto
                        case 736: // Ascoli Piceno
                        case 737: // Camerino
                        case 742: // Foligno
                        case 743: // Spoleto
                        case 744: // Terni
                        case 746: // Rieti
                        case 761: // Viterbo
                        case 763: // Orvieto
                        case 765: // Poggio Mirteto
                        case 766: // Civitavecchia
                        case 769: // Roma for mass call, until 31st December 2014
                        case 771: // Formia
                        case 773: // Latina
                        case 774: // Tivoli
                        case 775: // Frosinone
                        case 776: // Cassino
                        case 781: // Iglesias
                        case 782: // Lanusei
                        case 783: // Oristano
                        case 784: // Nuoro
                        case 785: // Macomer
                        case 789: // Olbia
                        case 823: // Caserta
                        case 824: // Benevento
                        case 825: // Avellino
                        case 827: // S. Angelo dei Lombardi
                        case 828: // Battipaglia
                        case 831: // Brindisi
                        case 832: // Lecce
                        case 833: // Gallipoli
                        case 835: // Matera
                        case 836: // Maglie
                        case 861: // Teramo
                        case 862: // L'Aquila
                        case 863: // Avezzano
                        case 864: // Sulmona
                        case 865: // Isernia
                        case 871: // Chieti
                        case 872: // Lanciano
                        case 873: // Vasto
                        case 874: // Campobasso
                        case 875: // Termoli
                        case 881: // Foggia
                        case 882: // S. Severo
                        case 883: // Andria
                        case 884: // Manfredonia
                        case 885: // Cerignola
                        case 921: // Cefalù
                        case 922: // Agrigento
                        case 923: // Trapani
                        case 924: // Alcamo
                        case 925: // Sciacca
                        case 931: // Siracusa
                        case 932: // Ragusa
                        case 933: // Caltagirone
                        case 934: // Caltanissetta
                        case 935: // Enna
                        case 941: // Patti
                        case 942: // Taormina
                        case 961: // Catanzaro
                        case 962: // Crotone
                        case 963: // Vibo Valentia
                        case 964: // Locri
                        case 965: // Reggio Calabria
                        case 966: // Palmi
                        case 967: // Soverato
                        case 968: // Lamezia Terme
                        case 971: // Potenza
                        case 972: // Melfi
                        case 973: // Lagonegro
                        case 974: // Vallo della Lucania
                        case 975: // Sala Consilina
                        case 976: // Muro Lucano
                        case 981: // Castrovillari
                        case 982: // Paola
                        case 983: // Rossano
                        case 984: // Cosenza
                        case 985: // Scalea
                            parse_out->parse_state = STATE_ITALY_SUB_1_6;
                            parse_out->field_pos = 0;
                            parse_out->service_type = TSERV_LAND;
                            break;
                        default:
                            set_parse_error(parse_out, ERROR_AREA_CODE_NOT_FOUND, 125);
                        break;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_AREA_CODE_DIGITS_INVALID, 126);
                break;
            }
        break;
        case STATE_ITALY_SUB_1_8:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0)
                        parse_out->parse_state = STATE_ITALY_SUB_1_8;
                    else
                        set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 127);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_ITALY_SUB1);
                    if (parse_out->field_pos < 8) {
                        parse_out->parse_state  = STATE_ITALY_SUB_1_8;
                    } else {
                        parse_out->parse_state  = STATE_EXTENSION_START;
                        parse_out->field_pos    = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 128);
                break;
            }
        break;
        case STATE_ITALY_SUB_1_7:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0)
                        parse_out->parse_state = STATE_ITALY_SUB_1_7;
                    else
                        set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 129);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_ITALY_SUB1);
                    if (parse_out->field_pos < 7) {
                        parse_out->parse_state  = STATE_ITALY_SUB_1_7;
                    } else {
                        parse_out->parse_state  = STATE_EXTENSION_START;
                        parse_out->field_pos    = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 130);
                break;
            }
        break;
        case STATE_ITALY_SUB_1_6:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0)
                        parse_out->parse_state = STATE_ITALY_SUB_1_6;
                    else
                        set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 131);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_ITALY_SUB1);
                    if (parse_out->field_pos < 6) {
                        parse_out->parse_state  = STATE_ITALY_SUB_1_6;
                    } else {
                        parse_out->parse_state  = STATE_EXTENSION_START;
                        parse_out->field_pos    = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 132);
                break;
            }
        break;
        case STATE_ITALY_CELL_PREFIX:
            switch (digit_value) {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_ITALY_AREA_CODE);
                    if (parse_out->field_pos < 3) {
                        parse_out->parse_state  = STATE_ITALY_CELL_PREFIX;
                    } else {
                        parse_out->parse_state  = STATE_ITALY_EX;
                        parse_out->field_pos    = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_AREA_CODE_DIGITS_INVALID, 133);
                break;
            }
        break;
        case STATE_ITALY_EX:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0)
                        parse_out->parse_state = STATE_ITALY_EX;
                    else
                        set_parse_error(parse_out, ERROR_EXCHANGE_CODE_DIGITS_INVALID, 134);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_ITALY_EX_CODE);
                    if (parse_out->field_pos < 3) {
                        parse_out->parse_state  = STATE_ITALY_EX;
                    } else {
                        parse_out->parse_state  = STATE_ITALY_SUB_1_4;
                        parse_out->field_pos    = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_EXCHANGE_CODE_DIGITS_INVALID, 135);
                break;
            }
        break;
        case STATE_ITALY_SUB_1_4:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0)
                        parse_out->parse_state = STATE_ITALY_SUB_1_4;
                    else
                        set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 136);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_ITALY_SUB1);
                    if (parse_out->field_pos < 4) {
                        parse_out->parse_state  = STATE_ITALY_SUB_1_4;
                    } else {
                        parse_out->parse_state  = STATE_EXTENSION_START;
                        parse_out->field_pos    = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 137);
                break;
            }
        break;
        case STATE_ITALY_SUB_2_3_3:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0)
                        parse_out->parse_state = STATE_ITALY_SUB_2_3_3;
                    else
                        set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, -1);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_ITALY_SUB1);
                    if (parse_out->field_pos < 3) {
                        parse_out->parse_state  = STATE_ITALY_SUB_2_3_3;
                    } else {
                        parse_out->parse_state  = STATE_ITALY_SUB_2_3;
                        parse_out->field_pos    = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, -1);
                break;
            }
        break;
        case STATE_ITALY_SUB_2_3:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0)
                        parse_out->parse_state = STATE_ITALY_SUB_2_3;
                    else
                        set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, -1);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_ITALY_SUB2);
                    if (parse_out->field_pos < 3) {
                        parse_out->parse_state  = STATE_ITALY_SUB_2_3;
                    } else {
                        parse_out->parse_state  = STATE_EXTENSION_START;
                        parse_out->field_pos    = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, -1);
                break;
            }
        break;
        case STATE_CALLING_CODE_ZONE4: // Europe
            switch (digit_value) {
                case 4: // United Kingdom
                    set_parse_out(digit_letter, parse_out, FIELD_CALLING_CODE);
                    parse_out->parse_state = STATE_UK_AREA_START;
                break;
                case 0: // Romania
                case 1: // Switzerland
                case 3: // Austria
                case 5: // Denmark
                case 6: // Sweden
                case 7: // Norway
                case 8: // Poland
                case 9: // Germany
                case 2:
                    set_parse_error(parse_out, ERROR_CALLING_CODE_NEED_CODE, 33);
                break;
                default:
                    set_parse_error(parse_out, ERROR_CALLING_CODE_DIGITS_INVALID, 34);
                break;
            }
        break;
        case STATE_UK_AREA_START:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    parse_out->parse_state = STATE_UK_AREA_START;
                break;
                case 1:
                case 2:
                    parse_out->service_type = TSERV_LAND;
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_UK_AREA_CODE);
                    parse_out->parse_state = STATE_UK_AREA_DIGIT2;
                    parse_out->field_value = digit_value;
                    if (digit_value == 9)
                        parse_out->service_type = TSERV_CHARGE;
                break;
                default:
                    set_parse_error(parse_out, ERROR_AREA_CODE_START_INVALID, 35);
                break;
            }
        break;
        case STATE_UK_AREA_DIGIT2:
            switch (digit_value) {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_UK_AREA_CODE);
                    field_value_add(parse_out);
                    switch (parse_out->field_value) {
                        case 20:
                        case 23:
                        case 24:
                        case 28:
                        case 29:
                            // 2+8 only: (02x) xxxx xxxx
                            parse_out->parse_state = STATE_UK_SUB_1_4_4;
                            parse_out->field_pos   = 0;
                        break;
                        case 70:
                        case 71:
                        case 73:
                        case 74:
                        case 75:
                        case 76:
                        case 77:
                        case 78:
                        case 79:
                            parse_out->parse_state = STATE_UK_AREA_1_4_6;
                        break;
                        default:
                            parse_out->parse_state = STATE_UK_AREA_DIGIT3;
                        break;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_AREA_CODE_DIGITS_INVALID, 36);
                break;
            }
        break;
        case STATE_UK_AREA_DIGIT3:
            switch (digit_value) {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_UK_AREA_CODE);
                    field_value_add(parse_out);
                    switch (parse_out->field_value) {
                        case 113:
                        case 114:
                        case 115:
                        case 116:
                        case 117:
                        case 118:
                        case 121:
                        case 131:
                        case 141:
                        case 151:
                        case 161:
                        case 191:
                        case 300:
                        case 302:
                        case 303:
                        case 306:
                        case 330:
                        case 331:
                        case 332:
                        case 333:
                        case 343:
                        case 344:
                        case 345:
                        case 370:
                        case 371:
                        case 372:
                            // 3+7 only: (011x) xxx xxxx
                            // 3+7 only: (01x1) xxx xxxx
                            parse_out->parse_state  = STATE_UK_SUB_1_3_4;
                            parse_out->field_pos    = 0;
                        break;
                        case 500:
                            set_parse_error(parse_out, ERROR_NUMBER_OBSOLETE, 37);
                        break;
                        case 800:
                        case 808:
                        case 870:
                            parse_out->service_type = TSERV_TOLLFREE;
                            parse_out->parse_state  = STATE_UK_SUB_1_3_4;
                            parse_out->field_pos    = 0;
                        break;
                        case 844:
                            parse_out->service_type = TSERV_CALLER_COST;
                            parse_out->parse_state  = STATE_UK_SUB_1_3_4;
                            parse_out->field_pos    = 0;
                        break;
                        case 845:
                            parse_out->service_type = TSERV_SHARED_COST;
                            parse_out->parse_state  = STATE_UK_SUB_1_3_4;
                            parse_out->field_pos    = 0;
                        break;
                        case 871:
                            parse_out->service_type = TSERV_CHARGE;
                            parse_out->parse_state  = STATE_UK_SUB_1_3_4;
                            parse_out->field_pos    = 0;
                        break;
                        case 843:
                        case 872:
                        case 900:
                        case 901:
                        case 902:
                        case 903:
                        case 904:
                        case 905:
                        case 906:
                        case 907:
                        case 908:
                        case 909:
                        case 911:
                        case 912:
                        case 913:
                        case 982:
                        case 983:
                        case 984:
                        case 989:
                            parse_out->parse_state  = STATE_UK_SUB_1_3_4;
                            parse_out->field_pos    = 0;
                        break;
                        break;
                        default:
                            parse_out->parse_state = STATE_UK_AREA_DIGIT4;
                        break;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_AREA_CODE_DIGITS_INVALID, 38);
                break;
            }
        break;
        case STATE_UK_AREA_DIGIT4:
            parse_out->alt_state = ALT_STATE_NULL;
            switch (digit_value) {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_UK_AREA_CODE);
                    field_value_add(parse_out);
                    switch (parse_out->field_value) {
                        case 1387:
                        case 1539:
                            // 4+6 areas where part of range is assigned as 5+5
                        case 1697:
                            // Mixed 4+6 and 4+5 areas where part of range is assigned as 5+5
                        case 1524:
                        case 1768:
                        case 1946:
                            parse_out->alt_state    = ALT_STATE_UK_4;
                            parse_out->parse_state  = STATE_UK_AREA_DIGIT5;
                        break;
                            // 10 or 9, 4+6 areas where part of range is assigned as mixed 5+5 and 5+4
                        case 1204:
                        case 1208:
                        case 1254:
                        case 1276:
                        case 1297:
                        case 1298:
                        case 1363:
                        case 1364:
                        case 1384:
                        case 1386:
                        case 1404:
                        case 1420:
                        case 1460:
                        case 1461:
                        case 1480:
                        case 1488:
                        case 1527:
                        case 1562:
                        case 1566:
                        case 1606:
                        case 1629:
                        case 1635:
                        case 1647:
                        case 1659:
                        case 1695:
                        case 1726:
                        case 1744:
                        case 1750:
                        case 1827:
                        case 1837:
                        case 1884:
                        case 1900:
                        case 1905:
                        case 1935:
                        case 1949:
                        case 1963:
                        case 1995:
                            // Mixed 4+6 and 4+5: (01xx xx) xxxxx
                            set_parse_error(parse_out, ERROR_NEED_LENGTH_CODE, 42);
                        break;
                        case 1632:
                            // fictional use, sub 960000 to 960999 are officially reserved
                            parse_out->parse_state = STATE_UK_SUB_1_6;
                            parse_out->field_pos = 0;
                        break;
                        case 5511:
                        case 5516:
                        case 5555:
                        case 5588:
                        case 5600:
                        case 5601:
                        case 5602:
                        case 5603:
                            parse_out->parse_state = STATE_UK_SUB_1_6;
                            parse_out->field_pos = 0;
                        break;
                        default:
                            parse_out->parse_state = STATE_UK_AREA_DIGIT5;
                        break;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_AREA_CODE_DIGITS_INVALID, 40);
                break;
            }
        break;
        case STATE_UK_AREA_DIGIT5:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    use_alt_state_uk(parse_out);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_UK_AREA_CODE);
                    field_value_add(parse_out);
                    switch (parse_out->field_value) {
                        case 13873:
                        case 15242:
                        case 15394:
                        case 15395:
                        case 15396:
                        case 16973:
                        case 16974:
                        case 17683:
                        case 17684:
                        case 17687:
                        case 19467:
                            // 5+5 only
                            parse_out->parse_state = STATE_UK_SUB_1_5;
                            parse_out->field_pos = 0;
                        break;
                        case 16977:
                            // Mixed 5+5 and 5+4: (0169 77) xxxx
                            set_parse_error(parse_out, ERROR_NEED_LENGTH_CODE, 41);
                        break;
                        default:
                            use_alt_state_uk(parse_out);
                        break;

                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_AREA_CODE_DIGITS_INVALID, 43);
                break;
            }
        break;
        case STATE_UK_AREA_1_4_6:
            switch (digit_value) {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_UK_AREA_CODE);
                    if (parse_out->field_pos < 4) {
                        parse_out->parse_state = STATE_UK_AREA_1_4_6;
                    } else {
                        parse_out->parse_state = STATE_UK_SUB_1_6;
                        parse_out->field_pos = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 44);
                break;
            }
        break;
        case STATE_UK_SUB_1_3_4:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0)
                        parse_out->parse_state = STATE_UK_SUB_1_3_4;
                    else
                        set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 45);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_UK_SUB1);
                    if (parse_out->field_pos < 3) {
                        parse_out->parse_state = STATE_UK_SUB_1_3_4;
                    } else {
                        parse_out->parse_state = STATE_UK_SUB_2_4;
                        parse_out->field_pos = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 46);
                break;
            }
        break;
        case STATE_UK_SUB_1_4_4:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0)
                        parse_out->parse_state = STATE_UK_SUB_1_4_4;
                    else
                        set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 47);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_UK_SUB1);
                    if (parse_out->field_pos < 4) {
                        parse_out->parse_state = STATE_UK_SUB_1_4_4;
                    } else {
                        parse_out->parse_state = STATE_UK_SUB_2_4;
                        parse_out->field_pos = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 48);
                break;
            }
        break;
        case STATE_UK_SUB_1_6:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0)
                        parse_out->parse_state  = STATE_UK_SUB_1_6;
                    else
                        set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 49);
               break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_UK_SUB1);
                    if (parse_out->field_pos < 6) {
                        parse_out->parse_state  = STATE_UK_SUB_1_6;
                    } else {
                        parse_out->parse_state  = STATE_EXTENSION_START;
                        parse_out->field_pos    = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 50);
                break;
            }
        break;
        case STATE_UK_SUB_2_4:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0)
                        parse_out->parse_state = STATE_UK_SUB_2_4;
                    else
                        set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_START2_INVALID, 51);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_UK_SUB2);
                    if (parse_out->field_pos == 1 && (
                        (
                            parse_out->digit_values[2] == 8 &&
                            parse_out->digit_values[3] == 0 &&
                            parse_out->digit_values[4] == 0 &&
                            parse_out->digit_values[5] == 1 &&
                            parse_out->digit_values[6] == 1 &&
                            parse_out->digit_values[7] == 1 &&
                            parse_out->digit_values[8] == 1
                        ) || (
                            parse_out->digit_values[2] == 8 && // 111 replaces this.
                            parse_out->digit_values[3] == 4 &&
                            parse_out->digit_values[4] == 5 &&
                            parse_out->digit_values[5] == 4 &&
                            parse_out->digit_values[6] == 6 &&
                            parse_out->digit_values[7] == 4 &&
                            parse_out->digit_values[8] == 7
                        )
                        )) {
                        parse_out->digit_fields[8] = FIELD_UK_SUB1;
                        parse_out->parse_state  = STATE_EXTENSION_START;
                        parse_out->field_pos    = 0;
                    } else if (parse_out->field_pos < 4) {
                        parse_out->parse_state  = STATE_UK_SUB_2_4;
                    } else {
                        parse_out->parse_state  = STATE_EXTENSION_START;
                        parse_out->field_pos    = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 52);
                break;
            }
        break;
        case STATE_UK_SUB_1_5:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0)
                        parse_out->parse_state = STATE_UK_SUB_1_5;
                    else
                        set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_START_INVALID, 53);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_UK_SUB1);
                    if (parse_out->field_pos < 5) {
                        parse_out->parse_state  = STATE_UK_SUB_1_5;
                    } else {
                        parse_out->parse_state  = STATE_EXTENSION_START;
                        parse_out->field_pos    = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 54);
                break;
            }
        break;
        case STATE_CALLING_CODE_ZONE5: // mostly Latin America
            switch (digit_value) {
                case 1: // Peru
                case 2: // Mexico
                case 3: // Cuba
                case 4: // Argentina
                case 5: // Brazil
                case 6: // Chile
                case 7: // Colombia
                case 8: // Venezuela
                case 0:
                case 9:
                    set_parse_error(parse_out, ERROR_CALLING_CODE_NEED_CODE, 55);
                break;
                default:
                    set_parse_error(parse_out, ERROR_CALLING_CODE_DIGITS_INVALID, 56);
                break;
            }
        break;
        case STATE_CALLING_CODE_ZONE6: // Southeast Asia and Oceania
            switch (digit_value) {
                case 1: // Australia
                    set_parse_out(digit_letter, parse_out, FIELD_CALLING_CODE);
                    parse_out->parse_state = STATE_AUSTRALIA_AREA_START;
                break;
                case 0: // Malaysia
                case 2: // Indonesia
                case 3: // Philippines
                case 4: // New Zealand
                case 5: // Singapore
                case 6: // Thailand
                case 7:
                case 8:
                case 9:
                    set_parse_error(parse_out, ERROR_CALLING_CODE_NEED_CODE, 57);
                break;
                default:
                    set_parse_error(parse_out, ERROR_CALLING_CODE_DIGITS_INVALID, 58);
                break;
            }
        break;
        case STATE_AUSTRALIA_AREA_START:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    parse_out->parse_state      = STATE_AUSTRALIA_AREA_START;
                break;
                case 2:
                case 3:
                case 7:
                case 8:
                    set_parse_out(digit_letter, parse_out, FIELD_AUSTRALIA_AREA_CODE);
                    parse_out->field_pos        = 0;
                    parse_out->parse_state      = STATE_AUSTRALIA_SUB_1_4_4;
                    parse_out->service_type     = TSERV_LAND;
                break;
                case 4:
                    set_parse_out(digit_letter, parse_out, FIELD_AUSTRALIA_AREA_CODE);
                    parse_out->field_pos        = 1;
                    parse_out->parse_state      = STATE_AUSTRALIA_AREA_1_3_3_3;
                    parse_out->service_type     = TSERV_CELL;
                break;
                default:
                    set_parse_error(parse_out, ERROR_AREA_CODE_DIGITS_INVALID, 59);
                break;
            }
        break;
        case STATE_AUSTRALIA_SUB_1_4_4:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0)
                        parse_out->parse_state = STATE_AUSTRALIA_SUB_1_4_4;
                    else
                        parse_out->parse_state = STATE_ERROR;
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_AUSTRALIA_SUB1);
                    if (parse_out->field_pos < 4) {
                        parse_out->parse_state  = STATE_AUSTRALIA_SUB_1_4_4;
                    } else {
                        parse_out->parse_state  = STATE_AUSTRALIA_SUB_2_4;
                        parse_out->field_pos    = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_START_INVALID, 60);
                break;
            }
        break;
        case STATE_AUSTRALIA_SUB_2_4:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0) {
                        parse_out->parse_state = STATE_AUSTRALIA_SUB_2_4;
                    } else {
                        set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_START2_INVALID, 61);
                    }
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_AUSTRALIA_SUB2);
                    if (parse_out->field_pos < 4) {
                        parse_out->parse_state  = STATE_AUSTRALIA_SUB_2_4;
                    } else {
                        parse_out->parse_state  = STATE_EXTENSION_START;
                        parse_out->field_pos    = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 62);
                break;
            }
        break;
        case STATE_AUSTRALIA_AREA_1_3_3_3:
            switch (digit_value) {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_AUSTRALIA_AREA_CODE);
                    if (parse_out->field_pos < 3) {
                        parse_out->parse_state  = STATE_AUSTRALIA_AREA_1_3_3_3;
                    } else {
                        parse_out->parse_state  = STATE_AUSTRALIA_SUB_1_3_3;
                        parse_out->field_pos    = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_AREA_CODE_DIGITS_INVALID, 63);
                break;
            }
        break;
        case STATE_AUSTRALIA_SUB_1_3_3:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0)
                        parse_out->parse_state = STATE_AUSTRALIA_SUB_1_3_3;
                    else
                        parse_out->parse_state = STATE_ERROR;
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_AUSTRALIA_SUB1);
                    if (parse_out->field_pos < 3) {
                        parse_out->parse_state  = STATE_AUSTRALIA_SUB_1_3_3;
                    } else {
                        parse_out->parse_state  = STATE_AUSTRALIA_SUB_2_3;
                        parse_out->field_pos    = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_START_INVALID, 64);
                break;
            }
        break;
        case STATE_AUSTRALIA_SUB_2_3:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0)
                        parse_out->parse_state = STATE_AUSTRALIA_SUB_2_3;
                    else
                        set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_START2_INVALID, 65);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_AUSTRALIA_SUB2);
                    if (parse_out->field_pos < 3) {
                        parse_out->parse_state  = STATE_AUSTRALIA_SUB_2_3;
                    } else {
                        parse_out->parse_state  = STATE_EXTENSION_START;
                        parse_out->field_pos    = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 66);
                break;
            }
        break;
        case STATE_RUSSIA_AREA_CODE_START: // former Soviet Union, plus Kazakhstan and Abkhazia
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    parse_out->parse_state = STATE_RUSSIA_AREA_CODE_START;
                break;
                case 0:
                case 1:
                case 2:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_RUSSIA_AREA_CODE);
                    parse_out->parse_state = STATE_RUSSIA_AREA_CODE_DIGITS;
                break;
                case 3:
                case 4:
                    set_parse_out(digit_letter, parse_out, FIELD_RUSSIA_AREA_CODE);
                    parse_out->parse_state      = STATE_RUSSIA_AREA_CODE_DIGITS;
                    parse_out->service_type     = TSERV_LAND;
                break;
                default:
                    set_parse_error(parse_out, ERROR_AREA_CODE_START_INVALID, 67);
                break;
            }
        break;
        case STATE_RUSSIA_AREA_CODE_DIGITS:
            switch (digit_value) {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_RUSSIA_AREA_CODE);
                    if (parse_out->field_pos < 3) {
                        parse_out->parse_state = STATE_RUSSIA_AREA_CODE_DIGITS;
                    } else {
                        parse_out->parse_state = STATE_RUSSIA_ZONE_START;
                        parse_out->service_type = TSERV_QUERY_RUSSIA;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_AREA_CODE_DIGITS_INVALID, 68);
                break;
            }
        break;
        case STATE_RUSSIA_ZONE_START:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    parse_out->parse_state = STATE_RUSSIA_ZONE_START;
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_RUSSIA_ZONE_CODE);
                    parse_out->parse_state = STATE_RUSSIA_ZONE_DIGITS;
                break;
                default:
                    set_parse_error(parse_out, ERROR_EXCHANGE_CODE_START_INVALID, 69);
                break;
            }
        break;
        case STATE_RUSSIA_ZONE_DIGITS:
            switch (digit_value) {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_RUSSIA_ZONE_CODE);
                    if (parse_out->field_pos < 3) {
                        parse_out->parse_state = STATE_RUSSIA_ZONE_DIGITS;
                    } else {
                        parse_out->parse_state = STATE_RUSSIA_SUB;
                        parse_out->field_pos = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_EXCHANGE_CODE_DIGITS_INVALID, 70);
                break;
            }
        break;
        case STATE_RUSSIA_SUB:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0 || parse_out->field_pos == 2)
                        parse_out->parse_state  = STATE_RUSSIA_SUB;
                    else
                        set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_START_INVALID, 71);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    if (parse_out->digit_fields[parse_out->digit_pos_next - 1] == FIELD_RUSSIA_SUB2) {
                        set_parse_out(digit_letter, parse_out, FIELD_RUSSIA_SUB2);
                        if (parse_out->field_pos < 2) {
                            parse_out->parse_state      = STATE_RUSSIA_SUB;
                        } else {
                            parse_out->parse_state      = STATE_EXTENSION_START;
                            parse_out->field_pos        = 0;
                        }
                    } else if (parse_out->field_pos < 2) {
                        set_parse_out(digit_letter, parse_out, FIELD_RUSSIA_SUB1);
                        parse_out->parse_state = STATE_RUSSIA_SUB;
                    } else {
                        set_parse_out(digit_letter, parse_out, FIELD_RUSSIA_SUB2);
                        parse_out->parse_state = STATE_RUSSIA_SUB;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_START_INVALID, 72);
                break;
            }
        break;
        case STATE_CALLING_CODE_ZONE8: // East Asia and special services
            switch (digit_value) {
                case 1: // Japan
                    set_parse_out(digit_letter, parse_out, FIELD_CALLING_CODE);
                    parse_out->parse_state = STATE_JAPAN_AREA_START;
                break;
                case 2: // South Korea
                case 4: // Vietnam
                case 6: // China
                case 0:
                case 3:
                case 5:
                case 7:
                case 8:
                case 9:
                    set_parse_error(parse_out, ERROR_CALLING_CODE_NEED_CODE, 73);
                break;
                default:
                    set_parse_error(parse_out, ERROR_CALLING_CODE_DIGITS_INVALID, 74);
                break;
            }
        break;
        case STATE_JAPAN_AREA_START:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    parse_out->parse_state = STATE_JAPAN_AREA_START;
                break;
                case 3: // Tokyo
                case 6: // Osaka
                    set_parse_out(digit_letter, parse_out, FIELD_JAPAN_AREA_CODE);
                    parse_out->parse_state      = STATE_JAPAN_SUB_1_4_4;
                    parse_out->field_pos        = 0;
                    parse_out->field_value      = digit_value;
                    parse_out->service_type     = TSERV_LAND;
                break;
                case 1:
                case 2:
                case 4:
                case 5:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_JAPAN_AREA_CODE);
                    parse_out->parse_state = STATE_JAPAN_AREA_DIGIT_2;
                    parse_out->field_value = digit_value;
                break;
                default:
                    parse_out->parse_state      = STATE_ERROR;
                    set_parse_error(parse_out, ERROR_AREA_CODE_START_INVALID, 75);
                break;
            }
        break;
        case STATE_JAPAN_AREA_DIGIT_2:
            switch (digit_value) {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_JAPAN_AREA_CODE);
                    field_value_add(parse_out);
                    switch (parse_out->field_value) {
                        case 80: // Cell phone
                        case 90: // Cell phone
                            parse_out->parse_state = STATE_JAPAN_SUB_1_4_4;
                            parse_out->field_pos = 0;
                            parse_out->service_type = TSERV_CELL;
                        break;
                        case 11: // Ebetsu- Hokkaido
                        case 19: // Iwate
                        case 22: // Miyagi
                        case 23: // Yamagata
                        case 24: // Fukushima
                        case 25: // Niigata
                        case 26: // Nagano
                        case 27: // Kitagunma District- Gunma
                        case 28: // Haga District- Tochigi
                        case 29: // Higashiibaraki District- Ibaraki
                        case 42: // Akiruno- Tokyo
                        case 43: // Chiba
                        case 44: // Kawasaki- Kanagawa
                        case 45: // Yokohama- Kanagawa
                        case 47: // Funabashi- Chiba
                        case 48: // Ageo- Saitama
                        case 52: // Aichi
                        case 53: // Hamamatsu- Shizuoka
                        case 54: // Shizuoka
                        case 58: // Gifu
                        case 59: // Age District- Mie
                        case 72: // Daito- Osaka
                        case 75: // Kyoto
                        case 76: // Ishikawa
                        case 77: // Shiga
                        case 78: // Akashi- Hyogo
                        case 82: // Hiroshima
                        case 86: // Okayama
                        case 87: // Kagawa
                        case 89: // Hojo- Ehime
                        case 92: // Fukuoka
                        case 93: // Kitakyushu- Fukuoka
                        case 95: // Nagasaki
                        case 96: // Kumamoto
                        case 98: // Okinawa
                        case 99: // Kagoshima
                            parse_out->alt_state    = ALT_STATE_JAPAN_LAND_2;
                            parse_out->parse_state  = STATE_JAPAN_AREA_DIGIT3;
                        break;
                        default:
                            parse_out->alt_state    = ALT_STATE_NULL;
                            parse_out->parse_state  = STATE_JAPAN_AREA_DIGIT3;
                        break;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_AREA_CODE_DIGITS_INVALID, 76);
                break;
            }
        break;
        case STATE_JAPAN_AREA_DIGIT3:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    use_alt_state_japan(parse_out);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_JAPAN_AREA_CODE);
                    field_value_add(parse_out);
                    switch (parse_out->field_value) {
                        case 120: // Toll free
                            parse_out->parse_state = STATE_JAPAN_SUB_1_3_3;
                            parse_out->field_pos = 0;
                            parse_out->service_type = TSERV_TOLLFREE;
                            break;
                        case 123: // Chitose- Hokkaido
                        case 125: // Akabira- Hokkaido
                        case 126: // Ishikari District- Hokkaido
                        case 133: // Atsuta District- Hokkaido
                        case 134: // Otaru- Hokkaido
                        case 135: // Furuu District- Hokkaido
                        case 136: // Abuta District- Hokkaido
                        case 138: // Hakodate- Hokkaido
                        case 142: // Abuta District- Hokkaido
                        case 143: // Muroran- Hokkaido
                        case 144: // Shiraoi District- Hokkaido
                        case 152: // Abashiri District- Hokkaido
                        case 153: // Akkeshi District- Hokkaido
                        case 154: // Akan District- Hokkaido
                        case 155: // Kasai District- Hokkaido
                        case 157: // Kitami- Hokkaido
                        case 162: // Teshio District- Hokkaido
                        case 164: // Fukagawa- Hokkaido
                        case 166: // Asahikawa- Hokkaido
                        case 167: // Ashibetsu- Hokkaido
                        case 172: // Aomori
                        case 173: // Goshogawara- Aomori
                        case 174: // Higashitsugaru District- Aomori
                        case 175: // Mutsu- Aomori
                        case 176: // Kamikita District- Aomori
                        case 177: // Aomori
                        case 178: // Hachinohe- Aomori
                        case 179: // Sannohe District- Aomori
                        case 182: // Hiraka District- Akita
                        case 183: // Ogachi District- Akita
                        case 184: // Honjo- Akita
                        case 185: // Minamiakita District- Akita
                        case 186: // Kazuno- Akita
                        case 187: // Omagari- Akita
                        case 188: // Akita
                        case 191: // Ichinoseki- Iwate
                        case 192: // Kesen District- Iwate
                        case 193: // Kamaishi- Iwate
                        case 194: // Kamihei District- Iwate
                        case 195: // Iwate
                        case 197: // Esashi- Iwate
                        case 198: // Hanamaki- Iwate
                        case 220: // Tome District- Miyagi
                        case 223: // Iwanuma- Miyagi
                        case 224: // Igu District- Miyagi
                        case 225: // Ishinomaki- Miyagi
                        case 226: // Kesennuma- Miyagi
                        case 228: // Kurihara District- Miyagi
                        case 229: // Furukawa- Miyagi
                        case 233: // Shinjo- Yamagata
                        case 234: // Akumi District- Yamagata
                        case 235: // Higashitagawa District- Yamagata
                        case 236: // Higashimurayama District- Yamagata
                        case 237: // Higashimurayama District- Yamagata
                        case 238: // Higashiokitama District- Yamagata
                        case 240: // Futaba District- Fukushima
                        case 241: // Kawanuma District- Fukushima
                        case 242: // Fukushima
                        case 243: // Fukushima
                        case 244: // Haramachi- Fukushima
                        case 246: // Iwaki- Fukushima
                        case 247: // Higashishirakawa District- Fukushima
                        case 248: // Iwase District- Fukushima
                        case 249: // Koriyama- Fukushima
                        case 250: // Gosen- Niigata
                        case 254: // Iwafune District- Niigata
                        case 255: // Arai- Niigata
                        case 256: // Kamo- Niigata
                        case 257: // Kariwa District- Niigata
                        case 258: // Kariwa District- Niigata
                        case 259: // Ryotsu- Niigata
                        case 260: // Shimoina District- Nagano
                        case 261: // Kitaazumi District- Nagano
                        case 263: // Higashichikuma District- Nagano
                        case 264: // Kiso District- Nagano
                        case 265: // Iida- Nagano
                        case 266: // Chino- Nagano
                        case 267: // Gunma
                        case 268: // Nagano
                        case 269: // Iiyama- Nagano
                        case 270: // Isesaki- Gunma
                        case 273: // Usui District- Gunma
                        case 274: // Chichibu District- Saitama
                        case 276: // Nitta District- Gunma
                        case 277: // Kiryu- Gunma
                        case 278: // Numata- Gunma
                        case 279: // Agatsuma District- Gunma
                        case 280: // Kitasaitama District- Saitama
                        case 282: // Tochigi
                        case 283: // Aso District- Tochigi
                        case 284: // Ashikaga- Tochigi
                        case 285: // Haga District- Tochigi
                        case 287: // Agatsuma District- Gunma
                        case 288: // Imaichi- Tochigi
                        case 289: // Kamitsuga District- Tochigi
                        case 291: // Kashima District- Ibaraki
                        case 292: // Naka District- Ibaraki
                        case 293: // Kitaibaraki- Ibaraki
                        case 294: // Hitachi- Ibaraki
                        case 296: // Kasama- Ibaraki
                        case 297: // Inashiki District- Ibaraki
                        case 298: // Inashiki District- Ibaraki
                        case 299: // Inashiki District- Ibaraki
                        case 422: // Chofu- Tokyo
                        case 423: // Fuchu- Tokyo
                        case 424: // Chofu- Tokyo
                        case 426: // Hachioji- Tokyo
                        case 427: // Kawasaki- Kanagawa
                        case 428: // Kitatsuru District- Yamanashi
                        case 429: // Hanno- Saitama
                        case 434: // Chiba
                        case 436: // Ichihara- Chiba
                        case 438: // Kisarazu- Chiba
                        case 439: // Futtsu- Chiba
                        case 460: // Ashigarashimo District- Kanagawa
                        case 462: // Aiko District- Kanagawa
                        case 463: // Hadano- Kanagawa
                        case 465: // Ashigarashimo District- Kanagawa
                        case 466: // Fujisawa- Kanagawa
                        case 467: // Ayase- Kanagawa
                        case 468: // Miura District- Kanagawa
                        case 470: // Awa District- Chiba
                        case 471: // Abiko- Chiba
                        case 474: // Chiba
                        case 475: // Chosei District- Chiba
                        case 476: // Inba District- Chiba
                        case 478: // Kashima District- Ibaraki
                        case 479: // Asahi- Chiba
                        case 480: // Kazo- Saitama
                        case 485: // Fukaya- Saitama
                        case 489: // Kitakatsushika District- Saitama
                        case 492: // Fujimi- Saitama
                        case 493: // Chichibu District- Saitama
                        case 494: // Chichibu District- Saitama
                        case 495: // Honjo- Saitama
                        case 532: // Toyohashi- Aichi
                        case 533: // Gamagori- Aichi
                        case 537: // Kakegawa- Shizuoka
                        case 538: // Fukuroi- Shizuoka
                        case 539: // Iwata District- Shizuoka
                        case 543: // Ihara District- Shizuoka
                        case 544: // Fujinomiya- Shizuoka
                        case 545: // Fuji- Shizuoka
                        case 547: // Haibara District- Shizuoka
                        case 548: // Haibara District- Shizuoka
                        case 550: // Gotenba- Shizuoka
                        case 551: // Kitakoma District- Yamanashi
                        case 552: // Higashiyatsushiro District- Yamanashi
                        case 553: // Yamanashi
                        case 554: // Minamitsuru District- Yamanashi
                        case 555: // Fujiyoshida- Yamanashi
                        case 556: // Minamikoma District- Yamanashi
                        case 557: // Atami- Shizuoka
                        case 558: // Kamo District- Shizuoka
                        case 559: // Mishima- Shizuoka
                        case 561: // Aichi
                        case 562: // Chita District- Aichi
                        case 563: // Nishio- Aichi hazu District- Aichi
                        case 564: // Higashikamo District- Aichi
                        case 565: // Higashikamo District- Aichi
                        case 566: // Anjo- Aichi
                        case 567: // Ama District- Aichi
                        case 568: // Inuyama- Aichi
                        case 569: // Chita District- Aichi
                        case 572: // Ena District- Gifu
                        case 573: // Ena District- Gifu
                        case 574: // Kamo District- Gifu
                        case 575: // Gujo District- Gifu
                        case 576: // Mashita District- Gifu
                        case 577: // Ono District- Gifu
                        case 578: // Yoshiki District- Gifu
                        case 581: // Motosu District- Gifu
                        case 583: // Kakamigahara- Gifu
                        case 584: // Anpachi District- Gifu
                        case 585: // Anpachi District- Gifu
                        case 586: // Haguri District- Aichi
                        case 587: // Inazawa- Aichi
                        case 593: // Mie
                        case 594: // Inabe District- Mie
                        case 595: // Ayama District- Mie
                        case 596: // Ise- Mie
                        case 598: // Ichishi District- Mie
                        case 599: // Ise- Mie
                        case 721: // Gose- Nara
                        case 722: // Sakai- Osaka
                        case 723: // Osaka
                        case 724: // Hannan- Osaka
                        case 725: // Izumi- Osaka
                        case 726: // Kyoto
                        case 727: // Ikeda- Osaka
                        case 729: // Fujiidera- Osaka
                        case 734: // Wakayama
                        case 735: // Minamimuro District- Mie
                        case 736: // Hashimoto- Wakayama
                        case 737: // Arida District- Wakayama
                        case 738: // Gobo- Wakayama
                        case 739: // Hidaka District- Wakayama
                        case 740: // Takashima District- Shiga
                        case 742: // Nara
                        case 743: // Ikoma District- Nara
                        case 744: // Kashihara- Nara
                        case 745: // Gose- Nara
                        case 747: // Yoshino District- Nara
                        case 748: // Echi District- Shiga
                        case 749: // Echi District- Shiga
                        case 761: // Komatsu- Ishikawa
                        case 763: // Higashitonami District- Toyama
                        case 764: // Toyama
                        case 765: // Kurobe- Toyama
                        case 766: // Toyama
                        case 767: // Hakui- Ishikawa
                        case 768: // Fugeshi District- Ishikawa
                        case 770: // Mikata District- Fukui
                        case 771: // Funai District- Kyoto
                        case 772: // Kumano District- Kyoto
                        case 773: // Amata District- Kyoto
                        case 774: // Joyo- Kyoto
                        case 775: // Kyoto
                        case 776: // Fukui
                        case 778: // Imadate District- Fukui
                        case 779: // Katsuyama- Fukui
                        case 790: // Kanzaki District- Hyogo
                        case 791: // Ibo District- Hyogo
                        case 792: // Himeji- Hyogo
                        case 794: // Kako District- Hyogo
                        case 795: // Hikami District- Hyogo
                        case 796: // Asago District- Hyogo
                        case 797: // Ashiya- Hyogo
                        case 798: // Ashiya- Hyogo
                        case 799: // Mihara District- Hyogo
                        case 820: // Kuga District- Yamaguchi
                        case 823: // Aki District- Hiroshima
                        case 824: // Futami District- Hiroshima
                        case 826: // Miyoshi- Hiroshima
                        case 827: // Iwakuni- Yamaguchi
                        case 829: // Saeki District- Hiroshima
                        case 832: // Shimonoseki- Yamaguchi
                        case 833: // Hikari- Yamaguchi
                        case 834: // Shinnanyo- Yamaguchi
                        case 835: // Hofu- Yamaguchi
                        case 836: // Asa District- Yamaguchi
                        case 837: // Nagato- Yamaguchi
                        case 838: // Abu District- Yamaguchi
                        case 839: // Yamaguchi
                        case 846: // Takehara- Hiroshima
                        case 847: // Ashina District- Hiroshima
                        case 848: // Mihara- Hiroshima
                        case 849: // Fukayasu District- Hiroshima
                        case 852: // Matsue- Shimane
                        case 853: // Hikawa District- Shimane
                        case 854: // Iishi District- Shimane
                        case 855: // Gotsu- Shimane
                        case 856: // Masuda- Shimane
                        case 857: // Tottori
                        case 858: // Kurayoshi- Tottori
                        case 859: // Hino District- Tottori
                        case 863: // Tamano- Okayama
                        case 865: // Asakuchi District- Okayama
                        case 866: // Ibara- Okayama
                        case 867: // Atetsu District- Okayama
                        case 868: // Katsuta District- Okayama
                        case 869: // Bizen- Okayama
                        case 875: // Kanonji- Kagawa
                        case 877: // Ayauta District- Kagawa
                        case 879: // Okawa District- Kagawa
                        case 880: // Hata District- Kochi
                        case 883: // Awa District- Tokushima
                        case 884: // Anan- Tokushima
                        case 886: // Tokushima
                        case 887: // Nagaoka District- Kochi
                        case 888: // Kochi
                        case 889: // Agawa District- Kochi
                        case 892: // Kamiukena District- Ehime
                        case 893: // Kita District- Ehime
                        case 894: // Higashiuwa District- Ehime
                        case 895: // Kitauwa District- Ehime
                        case 896: // Iyomishima- Ehime
                        case 897: // Niihama- Ehime
                        case 898: // Imabari- Ehime
                        case 933: // Kawanabe District- Kagoshima
                        case 940: // Munakata District- Fukuoka
                        case 942: // Chikugo- Fukuoka
                        case 943: // Yame District- Fukuoka
                        case 944: // Arao- Kumamoto
                        case 946: // Amagi- Fukuoka
                        case 947: // Nogata- Fukuoka
                        case 948: // Iizuka- Fukuoka
                        case 950: // Hirado- Nagasaki
                        case 952: // Saga
                        case 954: // Fujitsu District- Saga
                        case 955: // Higashimatsuura District- Saga
                        case 956: // Higashisonogi District- Nagasaki
                        case 957: // Higashisonogi District- Nagasaki
                        case 959: // Fukue- Nagasaki
                        case 964: // Amakusa District- Kumamoto
                        case 965: // Yatsushiro- Kumamoto
                        case 966: // Ashikita District- Kumamoto
                        case 967: // Aso District- Kumamoto
                        case 968: // Kamoto District- Kumamoto
                        case 969: // Amakusa District- Kumamoto
                        case 972: // Minamiamabe District- Oita
                        case 973: // Aso District- Kumamoto
                        case 974: // Naoiri District- Oita
                        case 975: // Oita
                        case 977: // Oita
                        case 978: // Bungotakada- Oita
                        case 979: // Buzen- Fukuoka
                        case 980: // Kunigami District- Okinawa
                        case 982: // Higashiusuki District- Miyazaki
                        case 983: // Higashiusuki District- Miyazaki
                        case 984: // Ebino- Miyazaki
                        case 985: // Miyazaki
                        case 986: // Kitamorokata District- Miyazaki
                        case 987: // Kushima- Miyazaki
                        case 993: // Hioki District- Kagoshima
                        case 994: // Kanoya- Kagoshima
                        case 995: // Aira District- Kagoshima
                        case 996: // Akune- Kagoshima
                        case 997: // Naze- Kagoshima
                            parse_out->alt_state    = ALT_STATE_JAPAN_LAND_3;
                            parse_out->parse_state  = STATE_JAPAN_AREA_DIGIT4;
                        break;
                        default:
                            parse_out->parse_state  = STATE_JAPAN_AREA_DIGIT4;
                        break;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_AREA_CODE_DIGITS_INVALID, 77);
                break;
            }
        break;
        case STATE_JAPAN_AREA_DIGIT4:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    use_alt_state_japan(parse_out);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_JAPAN_AREA_CODE);
                    field_value_add(parse_out);
                    switch (parse_out->field_value) {
                        case 1235: // Yubari- Hokkaido
                        case 1237: // Yubari District- Hokkaido
                        case 1238: // Yubari District- Hokkaido
                        case 1242: // Ashibetsu- Hokkaido
                        case 1266: // Bibai- Hokkaido
                        case 1267: // Iwamizawa- Hokkaido
                        case 1332: // Ishikari District- Hokkaido
                        case 1337: // Atsuta District- Hokkaido
                        case 1372: // Kayabe District- Hokkaido
                        case 1374: // Kayabe District- Hokkaido
                        case 1376: // Yamakoshi District- Hokkaido
                        case 1377: // Yamakoshi District- Hokkaido
                        case 1378: // Setana District- Hokkaido
                        case 1392: // Kamiiso District- Hokkaido
                        case 1394: // Matsumae District- Hokkaido
                        case 1395: // Hiyama District- Hokkaido
                        case 1396: // Hiyama District- Hokkaido
                        case 1397: // Okushiri District- Hokkaido
                        case 1398: // Kudo District- Hokkaido
                        case 1452: // Chitose- Hokkaido
                        case 1454: // Yufutsu District- Hokkaido
                        case 1456: // Niikappu District- Hokkaido
                        case 1457: // Saru District- Hokkaido
                        case 1462: // Urakawa District- Hokkaido
                        case 1463: // Mitsuishi District- Hokkaido
                        case 1464: // Niikappu District- Hokkaido
                        case 1466: // Horoizumi District- Hokkaido
                        case 1522: // Shari District- Hokkaido
                        case 1527: // Abashiri District- Hokkaido
                        case 1532: // Nemuro- Hokkaido
                        case 1537: // Notsuke District- Hokkaido
                        case 1538: // Menashi District- Hokkaido
                        case 1547: // Shiranuka District- Hokkaido
                        case 1548: // Kamikawa District- Hokkaido
                        case 1557: // Nakagawa District- Hokkaido
                        case 1558: // Hiroo District- Hokkaido
                        case 1562: // Ashoro District- Hokkaido
                        case 1564: // Kato District- Hokkaido
                        case 1566: // Kamikawa District- Hokkaido
                        case 1582: // Monbetsu- Hokkaido
                        case 1584: // Monbetsu District- Hokkaido
                        case 1586: // Monbetsu District- Hokkaido
                        case 1587: // Monbetsu District- Hokkaido
                        case 1588: // Monbetsu District- Hokkaido
                        case 1632: // Teshio District- Hokkaido
                        case 1634: // Esashi District- Hokkaido
                        case 1635: // Soya District- Hokkaido
                        case 1636: // Esashi District- Hokkaido
                        case 1638: // Rebun District- Hokkaido
                        case 1645: // Rumoi District- Hokkaido
                        case 1646: // Tomamae District- Hokkaido
                        case 1648: // Tomamae District- Hokkaido
                        case 1652: // Shibetsu- Hokkaido
                        case 1653: // Uryu District- Hokkaido
                        case 1654: // Nayoro- Hokkaido
                        case 1655: // Kamikawa District- Hokkaido
                        case 1656: // Nakagawa District- Hokkaido
                        case 1658: // Kamikawa District- Hokkaido
                        case 2549: // Higashikanbara District- Niigata
                        case 2559: // Higashikubiki District- Niigata
                        case 2579: // Kitauonuma District- Niigata
                        case 2955: // Higashiibaraki District- Ibaraki
                        case 2957: // Kuji District- Ibaraki
                        case 4992: // Oshima- Tokyocho
                        case 4994: // Miyake- Tokyocoho
                        case 4996: // Hachioji- Tokyo
                        case 4998: // Ogasawara- Tokyo
                        case 5312: // Atsumi District- Aichi
                        case 5313: // Atsumi District- Aichi
                        case 5362: // Shinshiro- Aichi
                        case 5363: // Minamishitara District- Aichi
                        case 5366: // Kitashitara District- Aichi
                        case 5367: // Kitashitara District- Aichi
                        case 5368: // Kitashitara District- Aichi
                        case 5613: // Aichi
                        case 5617: // Aichi
                        case 5675: // Ama District- Aichi
                        case 5679: // Ama District- Aichi
                        case 5747: // Kamo District- Gifu
                        case 5769: // Ono District- Gifu
                        case 5958: // Kameyama- Mie
                        case 5959: // Suzuka District- Mie
                        case 5972: // Owase- Mie
                        case 5973: // Kitamuro District- Mie
                        case 5974: // Kitamuro District- Mie
                        case 5978: // Kumano- Mie
                        case 5979: // Higashimuro District- Wakayama
                        case 5983: // Taki District- Mie
                        case 5984: // Ichishi District- Mie
                        case 5987: // Taki District- Mie
                        case 5988: // Taki District- Mie
                        case 5994: // Shima District- Mie
                        case 5995: // Shima District- Mie
                        case 5996: // Watarai District- Mie
                        case 5997: // Shima District- Mie
                        case 5998: // Shima District- Mie
                        case 7354: // Higashimuro District- Wakayama
                        case 7355: // Higashimuro District- Wakayama
                        case 7356: // Nishimuro District- Wakayama
                        case 7357: // Higashimuro District- Wakayama
                        case 7437: // Shijonawate- Osaka
                        case 7439: // Soekami District- Nara
                        case 7443: // Shiki District- Nara
                        case 7463: // Yoshino District- Nara
                        case 7472: // Gojo- Nara
                        case 7473: // Yoshino District- Nara
                        case 7617: // Enuma District- Ishikawa
                        case 7619: // Ishikawa
                        case 7797: // Asuwa District- Fukui
                        case 7912: // Aioi- Hyogo
                        case 7914: // Ako- Hyogo
                        case 7915: // Ako District- Hyogo
                        case 7932: // Ibo District- Hyogo
                        case 7933: // Shikama District- Hyogo
                        case 8207: // Oshima District- Yamaguchi
                        case 8247: // Shobara- Hiroshima
                        case 8248: // Hiba District- Hiroshima
                        case 8262: // Saeki District- Hiroshima
                        case 8275: // Iwakuni- Yamaguchi
                        case 8375: // Mine- Yamaguchi
                        case 8376: // Mine District- Yamaguchi
                        case 8387: // Abu District- Yamaguchi
                        case 8388: // Abu District- Yamaguchi
                        case 8395: // Abu District- Yamaguchi
                        case 8396: // Mine District- Yamaguchi
                        case 8452: // Innoshima- Hiroshima
                        case 8466: // Toyota District- Hiroshima
                        case 8472: // Sera District- Hiroshima
                        case 8473: // Kamo District- Hiroshima
                        case 8477: // Hiba District- Hiroshima
                        case 8478: // Jinseki District- Hiroshima
                        case 8487: // Mitsugi District- Hiroshima
                        case 8514: // Oki District- Shimane
                        case 8548: // Nima District- Shimane
                        case 8567: // Kanoashi District- Shimane
                        case 8636: // Okayama
                        case 8654: // Asakuchi District- Okayama
                        case 8687: // Aida District- Okayama
                        case 8692: // Oku District- Okayama
                        case 8695: // Akaiwa District- Okayama
                        case 8699: // Akaiwa District- Okayama
                        case 8802: // Hata District- Kochi
                        case 8808: // Tosashimizu- Kochi
                        case 8846: // Naka District- Tokushima
                        case 8847: // Kaifu District- Tokushima
                        case 8853: // Tokushima
                        case 8854: // Katsura District- Tokushima
                        case 8872: // Aki District- Kochi
                        case 8873: // Aki District- Kochi
                        case 8874: // Aki District- Kochi
                        case 8875: // Kami District- Kochi
                        case 9204: // Iki- Nagasaki
                        case 9205: // Shimoagata District- Nagasaki
                        case 9208: // Kamiagata District- Nagasaki
                        case 9302: // Miyako District- Fukuoka
                        case 9304: // Miyako District- Fukuoka
                        case 9305: // Chikujo District- Fukuoka
                        case 9437: // Asakura District- Fukuoka
                        case 9492: // Kurate District- Fukuoka
                        case 9493: // Kurate District- Fukuoka
                        case 9496: // Iizuka- Fukuoka
                        case 9546: // Fujitsu District- Saga
                        case 9676: // Aso District- Kumamoto
                        case 9697: // Amakusa District- Kumamoto
                        case 9737: // Kusu District- Oita
                        case 9786: // Higashikunisaki District- Oita
                        case 9802: // Shimajiri District- Okinawa
                        case 9807: // Hirara- Okinawa
                        case 9808: // Ishigaki- Okinawa
                        case 9912: // Kagoshima
                        case 9913: // Kagoshima
                        case 9942: // Kimotsuki District- Kagoshima
                        case 9952: // Isa District- Kagoshima
                        case 9969: // Satsuma District- Kagoshima
                        case 9972: // Kumage District- Kagoshima
                        case 9974: // Kumage District- Kagoshima
                        case 9977: // Oshima District- Kagoshima
                            parse_out->alt_state    = ALT_STATE_JAPAN_LAND_4;
                            parse_out->parse_state  = STATE_JAPAN_AREA_DIGIT5;
                        break;
                        default:
                            parse_out->parse_state  = STATE_JAPAN_AREA_DIGIT5;
                        break;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_AREA_CODE_DIGITS_INVALID, 78);
                break;
            }
        break;
        case STATE_JAPAN_AREA_DIGIT5:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    use_alt_state_japan(parse_out);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_JAPAN_AREA_CODE);
                    field_value_add(parse_out);
                    switch (parse_out->field_value) {
                        case 13379: // Hamamasu District- Hokkaido
                        case 15829: // Monbetsu District- Hokkaido
                        case 16528: // Kamikawa District- Hokkaido
                        case 16532: // Kamikawa District- Hokkaido
                        case 16534: // Kamikawa District- Hokkaido
                        case 53145: // Atsumi District- Aichi
                        case 53683: // Kitashitara District- Aichi
                        case 58138: // Motosu District- Gifu
                        case 58139: // Ibi District- Gifu
                        case 58157: // Mugi District- Gifu
                        case 58158: // Mugi District- Gifu
                        case 58689: // Hashima District- Gifu
                        case 59832: // Iinan District- Mie
                        case 59849: // Taki District- Mie
                        case 59856: // Ichishi District- Mie
                        case 59872: // Watarai District- Mie
                        case 73549: // Higashimuro District- Wakayama
                        case 74395: // Soraku District- Kyoto
                        case 82485: // Hiba District- Hiroshima
                        case 82486: // Hiba District- Hiroshima
                        case 82488: // Konu District- Hiroshima
                        case 84732: // Mitsugi District- Hiroshima
                        case 84762: // Konu District- Hiroshima
                        case 84767: // Konu District- Hiroshima
                        case 86542: // Asakuchi District- Okayama
                        case 86554: // Asakuchi District- Okayama
                        case 86926: // Oku District- Okayama
                        case 93032: // Miyako District- Fukuoka
                        case 93033: // Miyako District- Fukuoka
                            parse_out->parse_state = STATE_JAPAN_SUB_2_4;
                            parse_out->field_pos = 0;
                            parse_out->service_type = TSERV_LAND;
                        break;
                        default:
                            use_alt_state_japan(parse_out);
                        break;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_AREA_CODE_DIGITS_INVALID, 80);
                break;
            }
        break;
        case STATE_JAPAN_SUB_1_4_4:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0)
                        parse_out->parse_state = STATE_JAPAN_SUB_1_4_4;
                    else
                        set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 81);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_JAPAN_SUB1);
                    if (parse_out->field_pos < 4) {
                        parse_out->parse_state = STATE_JAPAN_SUB_1_4_4;
                    } else {
                        parse_out->parse_state = STATE_JAPAN_SUB_2_4;
                        parse_out->field_pos = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 82);
                break;
            }
        break;
        case STATE_JAPAN_SUB_1_3_4:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0)
                        parse_out->parse_state = STATE_JAPAN_SUB_1_3_4;
                    else
                        set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 83);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_JAPAN_SUB1);
                    if (parse_out->field_pos < 3) {
                        parse_out->parse_state = STATE_JAPAN_SUB_1_3_4;
                    } else {
                        parse_out->parse_state = STATE_JAPAN_SUB_2_4;
                        parse_out->field_pos = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 84);
                break;
            }
        break;
        case STATE_JAPAN_SUB_1_2_4:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0)
                        parse_out->parse_state = STATE_JAPAN_SUB_1_2_4;
                    else
                        set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 85);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_JAPAN_SUB1);
                    if (parse_out->field_pos < 2) {
                        parse_out->parse_state = STATE_JAPAN_SUB_1_2_4;
                    } else {
                        parse_out->parse_state = STATE_JAPAN_SUB_2_4;
                        parse_out->field_pos = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 89);
                break;
            }
        break;
        case STATE_JAPAN_SUB_1_1_4:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    parse_out->parse_state = STATE_JAPAN_SUB_1_1_4;
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_JAPAN_SUB1);
                    parse_out->parse_state = STATE_JAPAN_SUB_2_4;
                    parse_out->field_pos = 0;
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 116);
                break;
            }
        break;
        case STATE_JAPAN_SUB_2_4:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0)
                        parse_out->parse_state = STATE_JAPAN_SUB_2_4;
                    else
                        set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_START2_INVALID, 90);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_JAPAN_SUB2);
                    if (parse_out->field_pos < 4) {
                        parse_out->parse_state  = STATE_JAPAN_SUB_2_4;
                    } else {
                        parse_out->parse_state  = STATE_EXTENSION_START;
                        parse_out->field_pos    = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 91);
                break;
            }
        break;
        case STATE_JAPAN_SUB_1_6:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0)
                        parse_out->parse_state = STATE_JAPAN_SUB_1_6;
                    else
                        set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_START_INVALID, 92);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_JAPAN_SUB1);
                    if (parse_out->field_pos < 6) {
                        parse_out->parse_state  = STATE_JAPAN_SUB_1_6;
                    } else {
                        parse_out->parse_state  = STATE_EXTENSION_START;
                        parse_out->field_pos    = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 93);
                break;
            }
        break;
        case STATE_JAPAN_SUB_1_3_3:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0)
                        parse_out->parse_state = STATE_JAPAN_SUB_1_3_3;
                    else
                        set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 94);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_JAPAN_SUB1);
                    if (parse_out->field_pos < 3) {
                        parse_out->parse_state = STATE_JAPAN_SUB_1_3_3;
                    } else {
                        parse_out->parse_state = STATE_JAPAN_SUB_2_3;
                        parse_out->field_pos = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 95);
                break;
            }
        break;
        case STATE_JAPAN_SUB_2_3:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0)
                        parse_out->parse_state = STATE_JAPAN_SUB_2_3;
                    else
                        set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_START2_INVALID, 96);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_JAPAN_SUB2);
                    if (parse_out->field_pos < 3) {
                        parse_out->parse_state  = STATE_JAPAN_SUB_2_3;
                    } else {
                        parse_out->parse_state  = STATE_EXTENSION_START;
                        parse_out->field_pos    = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 97);
                break;
            }
        break;
        case STATE_CALLING_CODE_ZONE9: // mostly Asia
            switch (digit_value) {
                case 1: // India
                    set_parse_out(digit_letter, parse_out, FIELD_CALLING_CODE);
                    parse_out->parse_state = STATE_INDIA_AREA_START;
                break;
                case 0: // Turkey
                case 2: // Pakistan
                case 3: // Afghanistan
                case 4: // Sri Lanka
                case 5: // Myanmar
                case 8: // Iran
                case 6:
                case 7:
                case 9:
                    set_parse_error(parse_out, ERROR_CALLING_CODE_NEED_CODE, 98);
                break;
                default:
                    set_parse_error(parse_out, ERROR_CALLING_CODE_DIGITS_INVALID, 99);
                break;
            }
        break;
        case STATE_INDIA_AREA_START:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    parse_out->parse_state = STATE_INDIA_AREA_START;
                break;
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_INDIA_AREA_CODE);
                    parse_out->parse_state = STATE_INDIA_AREA_DIGIT2;
                    parse_out->field_value = digit_value;
                break;
                default:
                    set_parse_error(parse_out, ERROR_AREA_CODE_START_INVALID, 100);
                break;
            }
        break;
        case STATE_INDIA_AREA_DIGIT2:
            switch (digit_value) {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_INDIA_AREA_CODE);
                    field_value_add(parse_out);
                    switch (parse_out->field_value) {
                        case 11: // New Delhi
                        case 20: // Pune, Pune
                        case 22: // Mumbai, Mumbai
                        case 33: // Kolkata, Kolkata
                        case 40: // Hyderabad Loc, Hyderabad
                        case 44: // Chennai, Chennai
                        case 79: // Ahemdabad Local, Ahmedabad
                        case 80: // Bengaluru, Karnataka
                            parse_out->parse_state = STATE_INDIA_SUB_1_4_4;
                            parse_out->field_pos = 0;
                            parse_out->service_type = TSERV_LAND;
                        break;
                        default:
                            parse_out->parse_state = STATE_INDIA_AREA_DIGIT3;
                        break;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_AREA_CODE_DIGITS_INVALID, 101);
                break;
            }
        break;
        case STATE_INDIA_AREA_DIGIT3:
            switch (digit_value) {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_INDIA_AREA_CODE);
                    field_value_add(parse_out);
                    switch (parse_out->field_value) {
                        case 120: // Ghaziaba Dadri, Ghaziabad
                        case 121: // Meerut
                        case 122: // Hapur, Ghaziabad
                        case 124: // Gurgaon, Gurgaon
                        case 129: // Faridabad, Gurgaon
                        case 130: // Sonipat, Sonipat
                        case 131: // Muzaffar Nagar, Muzaffarnagar
                        case 132: // Saharanpur, Saharanpur
                        case 135: // Dehradun, Dehradun
                        case 141: // Jaipur, Jaipur
                        case 144: // Alwar
                        case 145: // Ajmer
                        case 151: // Bikaner (S), Bikaner
                        case 154: // Sriganganagar, Sriganganagar
                        case 160: // Kharar, Ropar
                        case 161: // Ludhiana, Ludhiana
                        case 164: // Bhatinda, Bhatinda
                        case 171: // Ambala, Ambala
                        case 172: // Chandigarh, Chandigarh
                        case 175: // Patiala, Patiala
                        case 177: // Shimla, Shimla
                        case 180: // Panipat, Karnal
                        case 181: // Jallandhar, Jalandhar
                        case 183: // Amritsar, Amritsar
                        case 184: // Karnal, Karnal
                        case 186: // Pathankot, Pathankot
                        case 191: // Jammu, Jammu
                        case 194: // Srinagar, Srinagar
                        case 212: // Chinchwad, Pune
                        case 215: // Navi Mumbai (Turbhe), Mumbai
                        case 217: // Sholapur, Sholapur
                        case 230: // Khadakwasala, Pune
                        case 231: // Kolhapur, Kolhapur
                        case 233: // Sangli, Sangli
                        case 240: // Aurangabad, Aurangabad
                        case 241: // Ahmednagar, Ahmednagar
                        case 250: // Bassein, Kalyan
                        case 251: // Kalyan, Kalyan
                        case 253: // Nasikcity, Nasik
                        case 257: // Jalgaon, Jalgaon
                        case 260: // Vapi, Valsad
                        case 261: // Surat, Surat
                        case 265: // Vadodara, Vadodara
                        case 268: // Nadiad, Nadiad
                        case 278: // Bhavnagar, Bhavnagar
                        case 281: // Rajkot, Rajkot
                        case 285: // Junagarh, Junagarh
                        case 286: // Porbander, Junagarh
                        case 288: // Jamnagar, Jamnagar
                        case 291: // Jodhpur (E), Jodhpur
                        case 294: // Girwa (Udaipur), Udaipur
                        case 326: // Dhanbad, Dhanbad
                        case 341: // Asansol, Asansol
                        case 342: // Burdwan, Asansol
                        case 343: // Durgapur, Asansol
                        case 353: // Siliguri, Darjeeling (Siliguri)
                        case 354: // Darjeeling, Darjeeling (Siliguri)
                        case 360: // Itanagar, Arunachal Pradesh (Zero)
                        case 361: // Guwahati, Guwahati
                        case 364: // Shillong, Meghalaya (Shillong)
                        case 368: // Passighat, Arunachal Pradesh (Zero)
                        case 369: // Mokokchung, Nagaland (Kohima)
                        case 370: // Kohima, Nagaland (Kohima)
                        case 372: // Lungleh, Mizoram (Aizwal)
                        case 373: // Dibrugarh, Tinsukia (Dibrugarh)
                        case 374: // Tinsukhia, Tinsukia (Dibrugarh)
                        case 376: // Jorhat, Jorhat
                        case 381: // Agartala, Tripura (Agartala)
                        case 385: // Imphal, Manipur (Imphal)
                        case 389: // Aizawal-I, Mizoram (Aizwal)
                        case 413: // Pondicherry, Pondichery
                        case 416: // Vellore, Vellore
                        case 421: // Tirupur, Coimbatore
                        case 422: // Coimbatore, Coimbatore
                        case 423: // Ootacamund, Ooty
                        case 424: // Erode, Erode
                        case 427: // Salem, Salem
                        case 431: // Trichy, Trichy
                        case 435: // Kumbakonam, Thanjavur
                        case 451: // Dindigul, Madurai
                        case 452: // Madurai, Madurai
                        case 461: // Tuticorin, Tuticorin
                        case 462: // Tirunelvelli, Tirunelvelli
                        case 466: // Shoranur, Palghat
                        case 467: // Kanhangad, Cannanore
                        case 469: // Tiruvalla, Tiruvalla
                        case 470: // Attingal, Thiruvananthapuram
                        case 471: // Thiruvananthapuram, Thiruvananthapuram
                        case 472: // Nedumandad, Thiruvananthapuram
                        case 474: // Quilon, Quilon
                        case 475: // Punalur, Quilon
                        case 476: // Karunagapally, Quilon
                        case 477: // Alleppey, Alleppy
                        case 478: // Shertallai, Alleppy
                        case 479: // Mavelikkara, Alleppy
                        case 480: // Irinjalakuda, Trichur
                        case 481: // Kottayam, Kottayam
                        case 483: // Manjeri, Calicut (Kozhikode)
                        case 484: // Ernakulam, Ernakulam
                        case 485: // Muvattupuzha, Ernakulam
                        case 487: // Trichur, Trichur
                        case 490: // Tellicherry, Cannanore
                        case 491: // Palghat, Palghat
                        case 494: // Tirur, Calicut (Kozhikode)
                        case 495: // Calicut, Calicut (Kozhikode)
                        case 496: // Badagara, Calicut (Kozhikode)
                        case 497: // Cannanore, Cannanore
                        case 512: // Kanpur, Kanpur
                        case 515: // Unnao, Unnao
                        case 522: // Lucknow, Lucknow
                        case 532: // Allahabad, Allahabad
                        case 535: // Raibareli, Raibareilly
                        case 542: // Varansi, Varansi
                        case 548: // Ghazipur, Ghazipur
                        case 551: // Gorakhpur, Gorakhpur
                        case 562: // Agra, Agra
                        case 565: // Mathura, Mathura
                        case 571: // Aligarh, Aligarh
                        case 581: // Bareilly, Bareilly
                        case 591: // Moradabad, Moradabad
                        case 595: // Rampur, Rampur
                        case 612: // Patna, Patna
                        case 621: // Muzaffarpur, Muzaffarpur
                        case 631: // Gaya, Gaya
                        case 641: // Bhagalpur, Bhagalpur
                        case 651: // Ranchi, Ranchi
                        case 657: // Jamshedpur, Jamshedpur
                        case 661: // Rourkela, Sundargarh (Rourkela)
                        case 663: // Sambalpur, Sambalpur
                        case 671: // Cuttack, Cuttack
                        case 674: // Bhubaneshwar, Bhubaneswar (Puri)
                        case 680: // Berhampur, Berhampur
                        case 712: // Nagpur, Nagpur
                        case 721: // Amravati, Amravati
                        case 724: // Akola, Akola
                        case 731: // Indore, Indore
                        case 733: // Khandwa, Khandwa
                        case 734: // Ujjain, Ujjain
                        case 744: // Ladpura (Kota), Kota
                        case 747: // Bundi, Bundi
                        case 751: // Gwalior, Gwalior
                        case 755: // Bhopal, Bhopal
                        case 761: // Jabalpur, Jabalpur
                        case 771: // Raipur, Raipur
                        case 788: // Durg, Durg
                        case 816: // Tumkur, Tumkur
                        case 820: // Udupi, Dakshin Kanada (Mangalore)
                        case 821: // Mysore, Mysore
                        case 824: // Mangalore, Dakshin Kanada (Mangalore)
                        case 831: // Belgaum, Belgaum
                        case 832: // Panji, Panji
                        case 836: // Hubli, Hubli
                        case 861: // Nellore, Nellore
                        case 863: // Guntur, Guntur
                        case 866: // Vijayawada, Vijayawada
                        case 870: // Warangal, Warangal
                        case 877: // Tirupathi, Chittoor
                        case 878: // Karimnagar, Karimnagar
                        case 883: // Rajahmundri, Rajahmundri
                        case 884: // Kakinada, Rajahmundri
                        case 891: // Visakhapatnam, Visakhapatnam
                            parse_out->parse_state = STATE_INDIA_SUB_1_3_4;
                            parse_out->field_pos = 0;
                            parse_out->service_type = TSERV_LAND;
                        break;
                        default:
                            parse_out->parse_state = STATE_INDIA_AREA_DIGIT4;
                        break;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_AREA_CODE_DIGITS_INVALID, 102);
                break;
            }
        break;
        case STATE_INDIA_AREA_DIGIT4:
            switch (digit_value) {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_INDIA_AREA_CODE);
                    field_value_add(parse_out);
                    switch (parse_out->field_value) {
                        case 1232: // Modinagar, Ghaziabad
                        case 1233: // Mawana, Meerut
                        case 1234: // Baghpat-Ii (Baraut), Meerut
                        case 1237: // Sardhana, Meerut
                        case 1250: // Charkhidadri, Rohtak
                        case 1251: // Jhajjar, Rohtak
                        case 1252: // Loharu, Rohtak
                        case 1253: // Tohsham, Rohtak
                        case 1254: // Bawanikhera, Rohtak
                        case 1255: // Siwani, Rohtak
                        case 1257: // Meham, Rohtak
                        case 1258: // Kalanaur, Rohtak
                        case 1259: // Kosli, Narnaul
                        case 1262: // Rohtak, Rohtak
                        case 1263: // Gohana, Sonipat
                        case 1267: // Nuh, Gurgaon
                        case 1268: // Ferojpur, Gurgaon
                        case 1274: // Rewari, Narnaul
                        case 1275: // Palwal, Gurgaon
                        case 1276: // Bahadurgarh, Rohtak
                        case 1281: // Jatusana, Narnaul
                        case 1282: // Narnaul, Narnaul
                        case 1284: // Bawal, Narnaul
                        case 1285: // Mohindergarh, Narnaul
                        case 1331: // Nakur (Gangoh), Saharanpur
                        case 1332: // Roorkee-I, Saharanpur
                        case 1334: // Roorkee-Ii (Hardwar), Saharanpur
                        case 1336: // Deoband, Saharanpur
                        case 1341: // Najibabad, Bijnore
                        case 1342: // Bijnore-I, Bijnore
                        case 1343: // Nagina, Bijnore
                        case 1344: // Dhampur, Bijnore
                        case 1345: // Bijnore-Ii (Chandpur), Bijnore
                        case 1346: // Pauri-Ii (Bubakhal), Kotdwara
                        case 1348: // Lansdown-Iii (Syunsi), Kotdwara
                        case 1360: // Chakrata (Dakpather), Dehradun
                        case 1363: // Karan Prayag, Kotdwara
                        case 1364: // Ukhimath (Guptkashi), Kotdwara
                        case 1368: // Pauri-I, Kotdwara
                        case 1370: // Deoprayag-Ii (Jakholi), Uttarkashi
                        case 1371: // Dunda, Uttarkashi
                        case 1372: // Chamoli, Kotdwara
                        case 1373: // Purola, Uttarkashi
                        case 1374: // Bhatwari-I (Uttarkashi), Uttarkashi
                        case 1375: // Rajgarhi, Uttarkashi
                        case 1376: // Tehri, Uttarkashi
                        case 1377: // Bhatwari-I (Gangotri), Uttarkashi
                        case 1378: // Deoprayag-I, Uttarkashi
                        case 1379: // Partapnagar, Uttarkashi
                        case 1381: // Joshimath-Ii (Badrinath), Kotdwara
                        case 1382: // Lansdown-Ii (Kotdwara), Kotdwara
                        case 1386: // Lansdown-I, Kotdwara
                        case 1389: // Joshimath-I, Kotdwara
                        case 1392: // Budhana, Muzaffarnagar
                        case 1396: // Jansath (Khatauli), Muzaffarnagar
                        case 1398: // Kairana (Shamli), Muzaffarnagar
                        case 1420: // Baswa (Bandikui), Jaipur
                        case 1421: // Kotputli, Jaipur
                        case 1422: // Viratnagar (Shahpura), Jaipur
                        case 1423: // Amber (Chomu), Jaipur
                        case 1424: // Phulera (E) (Renwal), Jaipur
                        case 1425: // Phulera (W) (Sambhar), Jaipur
                        case 1426: // Jamwa-Ramgarh (Achrol), Jaipur
                        case 1427: // Dausa, Jaipur
                        case 1428: // Dudu, Jaipur
                        case 1429: // Bassi, Jaipur
                        case 1430: // Phagi, Jaipur
                        case 1431: // Lalsot, Jaipur
                        case 1432: // Tonk (S), Tonk
                        case 1433: // Todaraisingh, Tonk
                        case 1434: // Deoli, Tonk
                        case 1435: // Tonk (N) (Piploo), Tonk
                        case 1436: // Uniayara, Tonk
                        case 1437: // Malpura, Tonk
                        case 1438: // Newai, Tonk
                        case 1460: // Kishangarhbas (Khairthal), Alwar
                        case 1461: // Bansur, Alwar
                        case 1462: // Beawar, Ajmer
                        case 1463: // Kishangarh (S), Ajmer
                        case 1464: // Rajgarh, Alwar
                        case 1465: // Thanaghazi, Alwar
                        case 1466: // Kekri (W) (Bhinai), Ajmer
                        case 1467: // Kekri (E), Ajmer
                        case 1468: // Ramgarh, Alwar
                        case 1469: // Tijara (S), Alwar
                        case 1470: // Dungla, Chittorgarh
                        case 1471: // Rashmi, Chittorgarh
                        case 1472: // Chittorgarh, Chittorgarh
                        case 1473: // Barisadri, Chittorgarh
                        case 1474: // Begun(N), Chittorgarh
                        case 1475: // Begun(S) (Rawatbhata), Chittorgarh
                        case 1476: // Kapasan, Chittorgarh
                        case 1477: // Nimbahera, Chittorgarh
                        case 1478: // Pratapgarh (N), Chittorgarh
                        case 1479: // Pratapgarh (S) (Arnod), Chittorgarh
                        case 1480: // Asind, Bhilwara
                        case 1481: // Raipur, Bhilwara
                        case 1482: // Bhilwara, Bhilwara
                        case 1483: // Hurda (Gulabpura), Bhilwara
                        case 1484: // Shahapura, Bhilwara
                        case 1485: // Jahazpur, Bhilwara
                        case 1486: // Mandal, Bhilwara
                        case 1487: // Banera, Bhilwara
                        case 1488: // Kotri, Bhilwara
                        case 1489: // Mandalgarh, Bhilwara
                        case 1491: // Nasirabad, Ajmer
                        case 1492: // Laxmangarh (Kherli), Alwar
                        case 1493: // Tijara (N) (Bhiwadi), Alwar
                        case 1494: // Behror, Alwar
                        case 1495: // Mandawar, Alwar
                        case 1496: // Sarwar, Ajmer
                        case 1497: // Kishangarh (N) (Roopangarh), Ajmer
                        case 1498: // Anupgarh (E), Sriganganagar
                        case 1499: // Sangaria, Sriganganagar
                        case 1501: // Srikaranpur, Sriganganagar
                        case 1502: // Nohar (W) (Jedasar), Sriganganagar
                        case 1503: // Sadulshahar, Sriganganagar
                        case 1504: // Bhadra, Sriganganagar
                        case 1505: // Padampur, Sriganganagar
                        case 1506: // Anupgarh (W) (Gharsana), Sriganganagar
                        case 1507: // Raisinghnagar, Sriganganagar
                        case 1508: // Suratgarh (N) (Goluwala), Sriganganagar
                        case 1509: // Suratgarh (S), Sriganganagar
                        case 1520: // Bikaner (N) (Chhatargarh), Bikaner
                        case 1521: // Bikaner(C) (Jaimalsar), Bikaner
                        case 1522: // Bikaner (E) (Jamsar), Bikaner
                        case 1523: // Bikaner (W) (Poogal), Bikaner
                        case 1526: // Lunkaransar-Ii (Mahajan), Bikaner
                        case 1527: // Lunkaransar-Iii (Rajasarb), Bikaner
                        case 1528: // Lunkaransar-Iv, Bikaner
                        case 1529: // Lunkaransar-I (Kanholi), Bikaner
                        case 1531: // Nokha (E), Bikaner
                        case 1532: // Nokha (W) (Nathusar), Bikaner
                        case 1533: // Kolayat-I (Goddo), Bikaner
                        case 1534: // Kolayat-Ii, Bikaner
                        case 1535: // Kolayat-Iii (Bajju), Bikaner
                        case 1536: // Kolayat-Iv (Daitra), Bikaner
                        case 1537: // Nohar(C) (Rawatsar), Sriganganagar
                        case 1539: // Tibbi, Sriganganagar
                        case 1552: // Hanumangarh, Sriganganagar
                        case 1555: // Nohar (E), Sriganganagar
                        case 1559: // Rajgarh, Churu
                        case 1560: // Sujangarh(C) (Bidasar), Churu
                        case 1561: // Taranagar, Churu
                        case 1562: // Churu, Churu
                        case 1563: // Sardarshahar (N)-Jaitsisar, Churu
                        case 1564: // Sardarshahar (S), Churu
                        case 1565: // Sridungargarh (N)-Dungargh, Churu
                        case 1566: // Sridungargarh (S) (Sudsar), Churu
                        case 1567: // Ratangarh, Churu
                        case 1568: // Sujangarh (E), Churu
                        case 1569: // Sujangarh (W) (Lalgarh), Churu
                        case 1570: // Laxmangarh (W) (Nechwa), Sikar
                        case 1571: // Fatehpur, Sikar
                        case 1572: // Sikar, Sikar
                        case 1573: // Laxmangarh (E), Sikar
                        case 1574: // Neem Ka Thana, Sikar
                        case 1575: // Srimadhopur, Sikar
                        case 1576: // Dantaramgarh (E) (Shyamji), Sikar
                        case 1577: // Dantaramgarh (W), Sikar
                        case 1580: // Deedwana, Nagaur
                        case 1581: // Ladnun, Nagaur
                        case 1582: // Nagaur (N), Nagaur
                        case 1583: // Jayal, Nagaur
                        case 1584: // Nagaur (E) (Mundwa Marwar), Nagaur
                        case 1585: // Nagaur (W) (Khinwsar), Nagaur
                        case 1586: // Nawa (Kuchamancity), Nagaur
                        case 1587: // Degana, Nagaur
                        case 1588: // Parbatsar (N) (Makrana), Nagaur
                        case 1589: // Parbatsar (S), Nagaur
                        case 1590: // Merta (E) (Merta-City), Nagaur
                        case 1591: // Merta (W) (Gotan), Nagaur
                        case 1592: // Jhunjhunu (S), Jhunjhunu
                        case 1593: // Khetri, Jhunjhunu
                        case 1594: // Udaipurwati, Jhunjhunu
                        case 1595: // Jhunjhunu (N) (Bissau), Jhunjhunu
                        case 1596: // Chirawa, Jhunjhunu
                        case 1624: // Jagraon, Ludhiana
                        case 1628: // Samrala, Ludhiana
                        case 1632: // Ferozepur, Ferozepur
                        case 1633: // Muktasar, Ferozepur
                        case 1634: // Abohar, Ferozepur
                        case 1635: // Kotkapura, Ferozepur
                        case 1636: // Moga, Ferozepur
                        case 1637: // Malaut, Ferozepur
                        case 1638: // Fazilka, Ferozepur
                        case 1639: // Faridakot, Ferozepur
                        case 1651: // Phulmandi, Bhatinda
                        case 1652: // Mansa, Bhatinda
                        case 1655: // Raman, Bhatinda
                        case 1659: // Sardulgarh, Bhatinda
                        case 1662: // Hissar, Hissar
                        case 1663: // Hansi, Hissar
                        case 1664: // Bhiwani, Rohtak
                        case 1666: // Sirsa, Hissar
                        case 1667: // Fatehabad, Hissar
                        case 1668: // Dabwali, Hissar
                        case 1669: // Adampur Mandi, Hissar
                        case 1672: // Sangrur, Sangrur
                        case 1675: // Malerkotla, Sangrur
                        case 1676: // Sunam, Sangrur
                        case 1679: // Barnala, Sangrur
                        case 1681: // Jind, Jind
                        case 1682: // Zira, Ferozepur
                        case 1683: // Julana, Jind
                        case 1684: // Narwana, Jind
                        case 1685: // Guruharsahai, Ferozepur
                        case 1686: // Safidon, Jind
                        case 1692: // Tohana, Hissar
                        case 1693: // Barwala, Hissar
                        case 1696: // Kalanwali, Hissar
                        case 1697: // Ratia, Hissar
                        case 1698: // Ellenabad, Hissar
                        case 1702: // Nahan, Solan
                        case 1704: // Paonta, Solan
                        case 1731: // Barara, Ambala
                        case 1732: // Jagadhari, Ambala
                        case 1733: // Kalka, Ambala
                        case 1734: // Naraingarh, Ambala
                        case 1735: // Chaaharauli, Ambala
                        case 1741: // Pehowa, Karnal
                        case 1743: // Cheeka, Karnal
                        case 1744: // Kurukshetra, Karnal
                        case 1745: // Nilokheri, Karnal
                        case 1746: // Kaithal, Karnal
                        case 1748: // Gharaunda, Karnal
                        case 1749: // Assandh, Karnal
                        case 1762: // Rajpura, Patiala
                        case 1763: // Sarhind, Patiala
                        case 1764: // Samana, Patiala
                        case 1765: // Nabha, Patiala
                        case 1781: // Rohru, Shimla
                        case 1782: // Rampur Bushahar, Shimla
                        case 1783: // Theog, Shimla
                        case 1785: // Pooh, Shimla
                        case 1786: // Kalpa, Shimla
                        case 1792: // Solan, Solan
                        case 1795: // Nalagarh, Solan
                        case 1796: // Arki, Solan
                        case 1799: // Rajgarh, Solan
                        case 1821: // Nakodar, Jalandhar
                        case 1822: // Kapurthala, Jalandhar
                        case 1823: // Nawanshahar, Jalandhar
                        case 1824: // Phagwara, Jalandhar
                        case 1826: // Phillaur, Jalandhar
                        case 1828: // Sultanpur Lodhi, Jalandhar
                        case 1851: // Patti, Amritsar
                        case 1852: // Taran Taran, Amritsar
                        case 1853: // Rayya, Amritsar
                        case 1858: // Ajnala, Amritsar
                        case 1859: // Goindwal, Amritsar
                        case 1870: // Jugial, Pathankot
                        case 1871: // Batala, Pathankot
                        case 1872: // Quadian, Pathankot
                        case 1874: // Gurdaspur, Pathankot
                        case 1875: // Dinanagar, Pathankot
                        case 1881: // Ropar, Ropar
                        case 1882: // Hoshiarpur, Hosiarpur
                        case 1883: // Dasua, Hosiarpur
                        case 1884: // Garhashanker, Hosiarpur
                        case 1885: // Balachaur, Hosiarpur
                        case 1886: // Tanda Urmar, Hosiarpur
                        case 1887: // Nangal, Ropar
                        case 1892: // Kangra (Dharamsala), Kangra (Dharamsala)
                        case 1893: // Nurpur, Kangra (Dharamsala)
                        case 1894: // Palampur, Kangra (Dharamsala)
                        case 1895: // Bharmour, Kangra (Dharamsala)
                        case 1896: // Churah (Tissa), Kangra (Dharamsala)
                        case 1897: // Pangi (Killar), Kandra (Dharamsala)
                        case 1899: // Chamba, Kangra (Dharamsala)
                        case 1900: // Lahul (Keylong), Kullu
                        case 1902: // Kullu, Kullu
                        case 1903: // Banjar, Kullu
                        case 1904: // Nirmand, Kullu
                        case 1905: // Mandi, Mandi
                        case 1906: // Spiti (Kaza), Kullu
                        case 1907: // Sundernagar, Mandi
                        case 1908: // Jogindernagar, Mandi
                        case 1909: // Udaipur, Kullu
                        case 1921: // Basholi, Jammu
                        case 1922: // Kathua, Jammu
                        case 1923: // Samba, Jammu
                        case 1924: // Akhnoor, Jammu
                        case 1931: // Kulgam, Srinagar
                        case 1932: // Anantnag, Srinagar
                        case 1933: // Pulwama, Srinagar
                        case 1936: // Pahalgam, Srinagar
                        case 1951: // Badgam, Srinagar
                        case 1952: // Baramulla, Srinagar
                        case 1954: // Sopore, Srinagar
                        case 1955: // Kupwara, Srinagar
                        case 1956: // Uri, Srinagar
                        case 1957: // Bandipur, Srinagar
                        case 1958: // Karnah, Srinagar
                        case 1960: // Nowshera, Rajouri
                        case 1962: // Rajouri, Rajouri
                        case 1964: // Kalakot, Rajouri
                        case 1965: // Poonch, Rajouri
                        case 1970: // Dehra Gopipur, Kangra (Dharamsala)
                        case 1972: // Hamirpur, Hamirpur
                        case 1975: // Una, Hamirpur
                        case 1976: // Amb, Hamirpur
                        case 1978: // Bilaspur, Hamirpur
                        case 1980: // Nobra, Leh
                        case 1981: // Nyoma, Leh
                        case 1982: // Leh, Leh
                        case 1983: // Zanaskar, Leh
                        case 1985: // Kargil, Leh
                        case 1990: // Ramnagar, Udhampur
                        case 1991: // Reasi, Udhampur
                        case 1992: // Udhampur, Udhampur
                        case 1995: // Kishtwar, Udhampur
                        case 1996: // Doda, Udhampur
                        case 1997: // Bedarwah, Udhampur
                        case 1998: // Ramban, Udhampur
                        case 1999: // Mahore, Udhampur
                        case 2111: // Indapur, Pune
                        case 2112: // Baramati, Pune
                        case 2113: // Bhor, Pune
                        case 2114: // Lonavala, Pune
                        case 2115: // Saswad, Pune
                        case 2117: // Daund, Pune
                        case 2118: // Walchandnagar, Pune
                        case 2119: // Kedgaon, Pune
                        case 2130: // Velhe, Pune
                        case 2132: // Junnar, Pune
                        case 2133: // Manchar, Pune
                        case 2135: // Rajgurunagar, Pune
                        case 2136: // Urlikanchan, Pune
                        case 2137: // Nahavara, Pune
                        case 2138: // Shirur, Pune
                        case 2139: // Pirangut, Pune
                        case 2140: // Mangaon, Pen
                        case 2141: // Alibagh, Pen
                        case 2142: // Pali, Pen
                        case 2143: // Pen, Pen
                        case 2144: // Murud, Pen
                        case 2145: // Mahad, Pen
                        case 2147: // Shrivardhan, Pen
                        case 2148: // Karjat, Pen
                        case 2149: // Mahasala, Pen
                        case 2160: // Sakarwadi, Satara
                        case 2161: // Vaduj, Satara
                        case 2162: // Satara, Satara
                        case 2163: // Koregaon, Satara
                        case 2164: // Karad, Satara
                        case 2165: // Dhiwadi, Satara
                        case 2166: // Phaltan, Satara
                        case 2167: // Wai, Satara
                        case 2168: // Mahabaleswar, Satara
                        case 2169: // Shirwal, Satara
                        case 2181: // Akkalkot, Sholapur
                        case 2182: // Karmala, Sholapur
                        case 2183: // Madha, Sholapur
                        case 2184: // Barsi, Sholapur
                        case 2185: // Malsuras, Sholapur
                        case 2186: // Pandharpur, Sholapur
                        case 2187: // Sangola, Sholapur
                        case 2188: // Mangalwedha, Sholapur
                        case 2189: // Mohol, Sholapur
                        case 2191: // Poladpur, Pen
                        case 2192: // Khopoli, Pen
                        case 2194: // Roha, Pen
                        case 2320: // Chandgad, Kolhapur
                        case 2321: // Radhanagar, Kolhapur
                        case 2322: // Shirol (Jalsingpur), Kolhapur
                        case 2323: // Ajara, Kolhapur
                        case 2324: // Hatkangale (Ichalkaranji), Kolhapur
                        case 2325: // Kagal (Murgud), Kolhapur
                        case 2326: // Gaganbavada, Kolhapur
                        case 2327: // Gadhinglaj, Kolhapur
                        case 2328: // Panhala, Kolhapur
                        case 2329: // Shahuwadi (Malakapur), Kolhapur
                        case 2341: // Kavathemankal, Sangli
                        case 2342: // Islampur, Sangli
                        case 2343: // Atpadi, Sangli
                        case 2344: // Jath, Sangli
                        case 2345: // Shirala, Sangli
                        case 2346: // Tasgaon, Sangli
                        case 2347: // Vita, Sangli
                        case 2350: // Madangad, Ratnagiri
                        case 2351: // Langa, Ratnagiri
                        case 2352: // Ratnagiri, Ratnagiri
                        case 2353: // Rajapur, Ratnagiri
                        case 2354: // Sanganeshwar (Deorukh), Ratnagiri
                        case 2355: // Chiplun, Ratnagiri
                        case 2356: // Khed, Ratnagiri
                        case 2357: // Malgund, Ratnagiri
                        case 2358: // Dapoli, Ratnagiri
                        case 2359: // Guhagar, Ratnagiri
                        case 2362: // Kudal, Kudal
                        case 2363: // Sawantwadi, Kudal
                        case 2364: // Deogad, Kudal
                        case 2365: // Malwan, Kudal
                        case 2366: // Vengurla, Kudal
                        case 2367: // Kankavali, Kudal
                        case 2371: // Wathar, Satara
                        case 2372: // Patan, Satara
                        case 2373: // Mahaswad, Satara
                        case 2375: // Pusegaon, Satara
                        case 2378: // Medha, Satara
                        case 2381: // Ahmedpur, Latur
                        case 2382: // Latur, Latur
                        case 2383: // Ausa, Latur
                        case 2384: // Nilanga, Latur
                        case 2385: // Udgir, Latur
                        case 2421: // Jamkhed, Ahmednagar
                        case 2422: // Shri Rampur, Ahmednagar
                        case 2423: // Koparagon, Ahmednagar
                        case 2424: // Akole, Ahmednagar
                        case 2425: // Sangamner, Ahmednagar
                        case 2426: // Rahuri, Ahmednagar
                        case 2427: // Newasa, Ahmednagar
                        case 2428: // Pathardi, Ahmednagar
                        case 2429: // Shevgaon, Ahmednagar
                        case 2430: // Sillod, Aurangabad
                        case 2431: // Paithan, Aurangabad
                        case 2432: // Aurangabad, Aurangabad
                        case 2433: // Gangapur, Aurangabad
                        case 2435: // Kannad, Aurangabad
                        case 2436: // Vijapur, Aurangabad
                        case 2437: // Khultabad, Aurangabad
                        case 2438: // Soyegaon, Aurangabad
                        case 2439: // Golegaon, Aurangabad
                        case 2441: // Ashti, Bhir
                        case 2442: // Bhir, Bhir
                        case 2443: // Manjalegaon, Bhir
                        case 2444: // Patoda, Bhir
                        case 2445: // Kaij, Bhir
                        case 2446: // Ambejogai, Bhir
                        case 2447: // Gevrai, Bhir
                        case 2451: // Pathari, Parbhani
                        case 2452: // Parbhani, Parbhani
                        case 2453: // Gangakhed, Parbhani
                        case 2454: // Basmatnagar, Parbhani
                        case 2455: // Kalamnuri, Parbhani
                        case 2456: // Hingoli, Parbhani
                        case 2457: // Jintdor, Parbhani
                        case 2460: // Delhi Tanda, Nanded
                        case 2461: // Mukhed, Nanded
                        case 2462: // Nanded, Nanded
                        case 2463: // Degloor, Nanded
                        case 2465: // Billoli, Nanded
                        case 2466: // Kandhar, Nanded
                        case 2467: // Bhokar, Nanded
                        case 2468: // Hadgaon, Nanded
                        case 2469: // Kinwat, Nanded
                        case 2471: // Tuljapur, Osmanabad
                        case 2472: // Osmanabad, Osmanabad
                        case 2473: // Kallam, Osmanabad
                        case 2475: // Omerga, Osmanabad
                        case 2477: // Paranda, Osmanabad
                        case 2478: // Bhoom, Osmanabad
                        case 2481: // Ner, Jalna
                        case 2482: // Jalna, Jalna
                        case 2483: // Ambad, Jalna
                        case 2484: // Partur, Jalna
                        case 2485: // Bhokardan, Jalna
                        case 2487: // Shrigonda, Ahmednagar
                        case 2488: // Parner, Ahmednagar
                        case 2489: // Karjat, Ahmednagar
                        case 2520: // Jawahar, Kalyan
                        case 2521: // Talasari, Kalyan
                        case 2522: // Bhiwandi, Kalyan
                        case 2524: // Murbad, Kalyan
                        case 2525: // Palghar, Kalyan
                        case 2526: // Wada, Kalyan
                        case 2527: // Shahapur, Kalyan
                        case 2528: // Dahanu, Kalyan
                        case 2529: // Mokhada, Kalyan
                        case 2550: // Niphad, Nasik
                        case 2551: // Sinnar, Nasik
                        case 2552: // Nandgaon, Nasik
                        case 2553: // Igatpuri, Nasik
                        case 2554: // Malegaon, Nasik
                        case 2555: // Satana, Nasik
                        case 2556: // Chanwad, Nasik
                        case 2557: // Dindori, Nasik
                        case 2558: // Peint, Nasik
                        case 2559: // Yeola, Nasik
                        case 2560: // Kusumba, Dhulia
                        case 2561: // Pimpalner, Dhulia
                        case 2562: // Dhule, Dhulia
                        case 2563: // Shirpur, Dhulia
                        case 2564: // Nandurbar, Dhulia
                        case 2565: // Shahada, Dhulia
                        case 2566: // Sindkheda, Dhulia
                        case 2567: // Taloda, Dhulia
                        case 2568: // Sakri, Dhulia
                        case 2569: // Navapur, Dhulia
                        case 2580: // Jamner, Jalgaon
                        case 2582: // Bhusawal, Jalgaon
                        case 2583: // Edalabad, Jalgaon
                        case 2584: // Raver, Jalgaon
                        case 2585: // Yawal, Jalgaon
                        case 2586: // Chopda, Jalgaon
                        case 2587: // Amalner, Jalgaon
                        case 2588: // Erandul, Jalgaon
                        case 2589: // Chalisgaon, Jalgaon
                        case 2591: // Manmad, Nasik
                        case 2592: // Kalwan, Nasik
                        case 2593: // Surgena, Nasik
                        case 2594: // Trimbak, Nasik
                        case 2595: // Dhadgaon, Dhulia
                        case 2596: // Pachora, Jalgaon
                        case 2597: // Parola, Jalgaon
                        case 2598: // Umrane, Nasik
                        case 2599: // Bhudargad (Gargoti), Kolhapur
                        case 2621: // Sayan, Surat
                        case 2622: // Bardoli, Surat
                        case 2623: // Mandvi, Surat
                        case 2624: // Fortsongadh, Surat
                        case 2625: // Valod, Surat
                        case 2626: // Vyara, Surat
                        case 2628: // Nizar, Surat
                        case 2629: // M.M.Mangrol, Surat
                        case 2630: // Bansada, Valsad
                        case 2631: // Ahwa, Valsad
                        case 2632: // Valsad, Valsad
                        case 2633: // Dharampur, Valsad
                        case 2634: // Billimora, Valsad
                        case 2637: // Navsari, Valsad
                        case 2640: // Rajpipla, Bharuch
                        case 2641: // Amod, Bharuch
                        case 2642: // Bharuch, Bharuch
                        case 2643: // Valia, Bharuch
                        case 2644: // Jambusar, Bharuch
                        case 2645: // Jhagadia, Bharuch
                        case 2646: // Ankleshwar, Bharuch
                        case 2649: // Dediapada, Bharuch
                        case 2661: // Naswadi, Vadodara
                        case 2662: // Padra, Vadodara
                        case 2663: // Dabhoi, Vadodara
                        case 2664: // Pavijetpur, Vadodara
                        case 2665: // Sankheda, Vadodara
                        case 2666: // Miyagam, Vadodara
                        case 2667: // Savli, Vadodara
                        case 2668: // Waghodia, Vadodara
                        case 2669: // Chhota Udaipur, Vadodara
                        case 2670: // Shehra, Godhra
                        case 2672: // Godhra, Godhra
                        case 2673: // Dahod, Godhra
                        case 2674: // Lunavada, Godhra
                        case 2675: // Santrampur, Godhra
                        case 2676: // Halol, Godhra
                        case 2677: // Limkheda, Godhra
                        case 2678: // Devgadhbaria, Godhra
                        case 2679: // Jhalod, Godhra
                        case 2690: // Balasinor, Nadiad
                        case 2691: // Kapad Wanj, Nadiad
                        case 2692: // Anand, Nadiad
                        case 2694: // Kheda, Nadiad
                        case 2696: // Borsad, Nadiad
                        case 2697: // Retlad, Nadiad
                        case 2698: // Khambat, Nadiad
                        case 2699: // Thasra, Nadiad
                        case 2711: // Barwala, Ahmedabad
                        case 2712: // Gandhi Nagar, Ahmedabad
                        case 2713: // Dhandhuka, Ahmedabad
                        case 2714: // Dholka, Ahmedabad
                        case 2715: // Viramgam, Ahmedabad
                        case 2716: // Dehgam, Ahmedabad
                        case 2717: // Sanand, Ahmedabad
                        case 2718: // Bareja, Ahmedabad
                        case 2733: // Harij, Mehsana
                        case 2734: // Chanasma, Mehsana
                        case 2735: // Deodar, Palanpur
                        case 2737: // Tharad, Palanpur
                        case 2738: // Santalpur, Palanpur
                        case 2739: // Vadgam, Palanpur
                        case 2740: // Vav, Palanpur
                        case 2742: // Palanpur, Palanpur
                        case 2744: // Deesa, Palanpur
                        case 2746: // Radhanpur, Palanpur
                        case 2747: // Thara, Palanpur
                        case 2748: // Dhanera, Palanpur
                        case 2749: // Danta, Palanpur
                        case 2751: // Chotila, Surendranagar
                        case 2752: // Surendranagar, Surendranagar
                        case 2753: // Limbdi, Surendranagar
                        case 2754: // Dhrangadhra, Surendranagar
                        case 2755: // Sayla, Surendranagar
                        case 2756: // Muli, Surendranagar
                        case 2757: // Dasada, Surendranagar
                        case 2758: // Halvad, Surendranagar
                        case 2759: // Lakhtar, Surendranagar
                        case 2761: // Kheralu, Mehsana
                        case 2762: // Mehsana, Mehsana
                        case 2763: // Vijapur, Mehsana
                        case 2764: // Kalol, Mehsana
                        case 2765: // Visnagar, Mehsana
                        case 2766: // Patan, Mehsana
                        case 2767: // Sidhpur, Mehsana
                        case 2770: // Prantij, Himatnagar
                        case 2771: // Bhiloda, Himatnagar
                        case 2772: // Himatnagar, Himatnagar
                        case 2773: // Malpur, Himatnagar
                        case 2774: // Modasa, Himatnagar
                        case 2775: // Khedbrahma, Himatnagar
                        case 2778: // Idar, Himatnagar
                        case 2779: // Bayad, Himatnagar
                        case 2791: // Babra, Amreli
                        case 2792: // Amreli, Amreli
                        case 2793: // Damnagar, Amreli
                        case 2794: // Rajula, Amreli
                        case 2795: // Kodinar, Junagarh
                        case 2796: // Kunkawav, Amreli
                        case 2797: // Dhari, Amreli
                        case 2801: // Ranavav, Junagarh
                        case 2803: // Khavda, Bhuj
                        case 2804: // Kutiyana, Junagarh
                        case 2806: // Gogodar, Bhuj
                        case 2808: // Sumrasar, Bhuj
                        case 2820: // Paddhari, Rajkot
                        case 2821: // Jasdan, Rajkot
                        case 2822: // Morvi, Rajkot
                        case 2823: // Jetpur, Rajkot
                        case 2824: // Dhoraji, Rajkot
                        case 2825: // Gondal, Rajkot
                        case 2826: // Upleta, Rajkot
                        case 2827: // Kotdasanghani, Rajkot
                        case 2828: // Wankaner, Rajkot
                        case 2829: // Maliya Miyana, Rajkot
                        case 2830: // Rahpar, Bhuj
                        case 2831: // Nalia, Bhuj
                        case 2832: // Bhuj, Bhuj
                        case 2833: // Khambhalia, Jamnagar
                        case 2834: // Kutchmandvi, Bhuj
                        case 2835: // Nakhatrana, Bhuj
                        case 2836: // Anjar (Gandhidham), Bhuj
                        case 2837: // Bhachav, Bhuj
                        case 2838: // Mundra, Bhuj
                        case 2839: // Lakhpat, Bhuj
                        case 2841: // Vallabhipur, Bhavnagar
                        case 2842: // Talaja, Bhavnagar
                        case 2843: // Gariadhar, Bhavnagar
                        case 2844: // Mahuva, Bhavnagar
                        case 2845: // Savarkundla, Amreli
                        case 2846: // Sihor, Bhavnagar
                        case 2847: // Gadhada, Bhavnagar
                        case 2848: // Palitana, Bhavnagar
                        case 2849: // Botad, Bhavnagar
                        case 2870: // Malia-Hatina, Junagarh
                        case 2871: // Keshod, Junagarh
                        case 2872: // Vanthali, Junagarh
                        case 2873: // Visavadar, Junagarh
                        case 2874: // Manavadar, Junagarh
                        case 2875: // Una-Diu, Junagarh
                        case 2876: // Veraval, Junagarh
                        case 2877: // Talala, Junagarh
                        case 2878: // Mangrol, Junagarh
                        case 2891: // Jamkalyanpur, Jamnagar
                        case 2892: // Okha, Jamnagar
                        case 2893: // Jodia, Jamnagar
                        case 2894: // Kalawad, Jamnagar
                        case 2895: // Lalpur, Jamnagar
                        case 2896: // Bhanvad, Jamnagar
                        case 2897: // Dhrol, Jamnagar
                        case 2898: // Jamjodhpur, Jamnagar
                        case 2900: // Siwana (E) (Samdari), Barmer
                        case 2901: // Siwana (W), Barmer
                        case 2902: // Barmer (N) (Kanot), Barmer
                        case 2903: // Chohtan (S) (Gangasar), Barmer
                        case 2904: // Deogarh, Udaipur
                        case 2905: // Sarada (Chawand), Udaipur
                        case 2906: // Salumber, Udaipur
                        case 2907: // Kherwara, Udaipur
                        case 2908: // Amet, Udaipur
                        case 2909: // Bhim (S) (Dawer), Udaipur
                        case 2920: // Bilara (N) (Bhopalgarh), Jodhpur
                        case 2921: // Phalodi (N) (Bap), Jodhpur
                        case 2922: // Osian (N), Jodhpur
                        case 2923: // Phalodi (E) (Lohawat), Jodhpur
                        case 2924: // Phalodi (W) (Baroo), Jodhpur
                        case 2925: // Phalodi (S), Jodhpur
                        case 2926: // Osian (S) (Mathania), Jodhpur
                        case 2927: // Osian (E) (Dhanwara), Jodhpur
                        case 2928: // Shergarh (N) (Deechu), Jodhpur
                        case 2929: // Shergarh (N) (Balesar), Jodhpur
                        case 2930: // Bilara (S) (Piparcity), Jodhpur
                        case 2931: // Jodhpur (W) (Jhanwar), Jodhpur
                        case 2932: // Pali (S), Pali (Marwar)
                        case 2933: // Bali (N) (Sumerpur), Pali (Marwar)
                        case 2934: // Desuri (Rani), Pali (Marwar)
                        case 2935: // Marwar-Jn, Pali (Marwar)
                        case 2936: // Pali (N) (Rohat), Pali (Marwar)
                        case 2937: // Raipur, Pali (Marwar)
                        case 2938: // Bali (S), Pali (Marwar)
                        case 2939: // Jaitaran, Pali (Marwar)
                        case 2950: // Dhariawad, Udaipur
                        case 2951: // Bhim (N), Udaipur
                        case 2952: // Rajsamand (Kankorli), Udaipur
                        case 2953: // Nathdwara, Udaipur
                        case 2954: // Kumbalgarh (Charbhujaji), Udaipur
                        case 2955: // Malvi (Fatehnagar), Udaipur
                        case 2956: // Gogunda, Udaipur
                        case 2957: // Vallabhnagar, Udaipur
                        case 2958: // Kotra, Udaipur
                        case 2959: // Jhadol, Udaipur
                        case 2960: // Sojat (Sojat-City), Pali (Marwar)
                        case 2961: // Ghatol, Banswara
                        case 2962: // Banswara, Banswara
                        case 2963: // Gerhi (Partapur), Banswara
                        case 2964: // Dungarpur, Banswara
                        case 2965: // Kushalgarh, Banswara
                        case 2966: // Sagwara, Banswara
                        case 2967: // Aspur, Banswara
                        case 2968: // Bagidora, Banswara
                        case 2969: // Bhinmal (N), Sirohi (Abu Road)
                        case 2970: // Sanchore (W) (Hadecha), Sirohi (Abu Road)
                        case 2971: // Pindwara, Sirohi (Abu Road)
                        case 2972: // Sirohi, Sirohi (Abu Road)
                        case 2973: // Jalore, Sirohi (Abu Road)
                        case 2974: // Abu Road, Sirohi (Abu Road)
                        case 2975: // Reodar, Sirohi (Abu Road)
                        case 2976: // Sheoganj (Posaliyan), Sirohi (Abu Road)
                        case 2977: // Jalore (W) (Sayla), Sirohi (Abu Road)
                        case 2978: // Ahore, Sirohi (Abu Road)
                        case 2979: // Sanchore (E), Sirohi (Abu Road)
                        case 2980: // Pachpadra (E) (Korna), Barmer
                        case 2981: // Sheo (W) (Harsani), Barmer
                        case 2982: // Barmer(C), Barmer
                        case 2983: // Barmer (E) (Gudda), Barmer
                        case 2984: // Barmer (S) Sindari, Barmer
                        case 2985: // Barmer (W) (Ramsar), Barmer
                        case 2986: // Barmer (Sw) (Dhorimanna), Barmer
                        case 2987: // Sheo (E), Barmer
                        case 2988: // Pachpadra (W) (Balotra), Barmer
                        case 2989: // Chohtan (N), Barmer
                        case 2990: // Bhinmal (S) (Jasawantpura), Sirohi (Abu Road)
                        case 2991: // Jaisalmer-1 (Ramgarh), Jaisalmer
                        case 2992: // Jaisalmer-11 (Jaisalmer), Jaisalmer
                        case 2993: // Jaisalmer-12 (Devikot), Jaisalmer
                        case 2994: // Pokran-4 (Pokran), Jaisalmer
                        case 2995: // Pokran-1 (Nachna), Jaisalmer
                        case 2996: // Pokran-3 (Loharki), Jaisalmer
                        case 2997: // Jaisalmer-7 (Mohargarh), Jaisalmer
                        case 2998: // Jaisalmer-5 (Khuiyals), Jaisalmer
                        case 2999: // Jaisalmer-3 (Nehdai), Jaisalmer
                        case 3010: // Jaisalmer-4 (Shahgarh), Jaisalmer
                        case 3011: // Jaisalmer-6 (Pasewar), Jaisalmer
                        case 3012: // Jaisalmer-8 (Mehsana), Jaisalmer
                        case 3013: // Jaisalmer-9 (Dhanaua), Jaisalmer
                        case 3014: // Jaisalmer-10 (Khuri), Jaisalmer
                        case 3015: // Jaisalmer-13 (Myajlar), Jaisalmer
                        case 3016: // Jaisalmer-14 (Jheenjaniyali), Jaisalmer
                        case 3017: // Pokran-2 (Madasar), Jaisalmer
                        case 3018: // Jaisalmer-2 (Sadhna), Jaisalmer
                        case 3019: // Pokran-5 (Phalsoond), Jaisalmer
                        case 3174: // Diamond Harbour, Kolkata
                        case 3192: // Andaman Islands, Andaman & Nicobar
                        case 3193: // Nicobar Islands, Andaman & Nicobar
                        case 3210: // Kakdwip, Kolkata
                        case 3211: // Arambag, Kolkata
                        case 3212: // Champadanga, Kolkata
                        case 3213: // Dhaniakhali, Kolkata
                        case 3214: // Jagatballavpur, Kolkata
                        case 3215: // Bongoan, Kolkata
                        case 3216: // Habra, Kolkata
                        case 3217: // Basirhat, Kolkata
                        case 3218: // Canning, Kolkata
                        case 3220: // Contai, Midnapur (Kharagpur)
                        case 3221: // Jhargram, Midnapur (Kharagpur)
                        case 3222: // Kharagpur, Midnapur (Kharagpur)
                        case 3223: // Nayagarh (Kultikri), Midnapur (Kharagpur)
                        case 3224: // Haldia, Midnapur (Kharagpur)
                        case 3225: // Ghatal, Midnapur (Kharagpur)
                        case 3227: // Amlagora, Midnapur (Kharagpur)
                        case 3228: // Tamluk, Midnapur (Kharagpur)
                        case 3229: // Dantan, Midnapur (Kharagpur)
                        case 3241: // Gangajalghati, Bankura
                        case 3242: // Bankura, Bankura
                        case 3243: // Khatra, Bankura
                        case 3244: // Bishnupur, Bankura
                        case 3251: // Adra, Purulia
                        case 3252: // Purulia, Purulia
                        case 3253: // Manbazar, Purulia
                        case 3254: // Jhalda, Purulia
                        case 3451: // Seharabazar, Asansol
                        case 3452: // Guskara, Asansol
                        case 3453: // Katwa, Asansol
                        case 3454: // Kalna, Asansol
                        case 3461: // Rampur Hat, Suri
                        case 3462: // Suri, Suri
                        case 3463: // Bolpur, Suri
                        case 3465: // Nalhati, Suri
                        case 3471: // Karimpur, Krishnanagar
                        case 3472: // Krishna Nagar, Krishnanagar
                        case 3473: // Ranaghat, Krishnanagar
                        case 3474: // Bethuadahari, Krishnanagar
                        case 3481: // Islampur (M), Berhampur
                        case 3482: // Berhampur, Berhampur
                        case 3483: // Murshidabad (Jiaganj), Berhampur
                        case 3484: // Kandi, Berhampur
                        case 3485: // Dhuliyan, Berhampur
                        case 3511: // Bubulchandi, Malda
                        case 3512: // Malda, Malda
                        case 3513: // Harishchandrapur, Malda
                        case 3521: // Gangarampur, Balurghat (Raiganj)
                        case 3522: // Balurghat, Balurghat (Raiganj)
                        case 3523: // Raiganj, Balurghat (Raiganj)
                        case 3524: // Harirampur, Balurghat (Raiganj)
                        case 3525: // Dalkhola, Balurghat (Raiganj)
                        case 3526: // Islampur (Nd), Balurghat (Raiganj)
                        case 3552: // Kalimpong, Darjeeling (Siliguri)
                        case 3561: // Jalpaiguri, Jalpaiguri
                        case 3562: // Mal Bazar, Jalpaiguri
                        case 3563: // Birpara, Jalpaiguri
                        case 3564: // Alipurduar, Jalpaiguri
                        case 3565: // Nagarakata, Jalpaiguri
                        case 3566: // Kalchini, Jalpaiguri
                        case 3581: // Dinhata, Coochbehar
                        case 3582: // Coochbehar, Coochbehar
                        case 3583: // Mathabhanga, Coochbehar
                        case 3584: // Mekhliganj, Coochbehar
                        case 3592: // Gangtok, Gangtok
                        case 3595: // Gauzing (Nayabazar), Gangtok
                        case 3621: // Boko, Guwahati
                        case 3623: // Barama, Guwahati
                        case 3624: // Nalbari, Guwahati
                        case 3637: // Cherrapunjee, Meghalaya (Shillong)
                        case 3638: // Nongpoh, Meghalaya (Shillong)
                        case 3639: // Baghmara, Meghalaya (Shillong)
                        case 3650: // Dadengiri (Phulbari), Meghalaya (Shillong)
                        case 3651: // Tura, Meghalaya (Shillong)
                        case 3652: // Jowai, Meghalaya (Shillong)
                        case 3653: // Amlaren (Dawki), Meghalaya (Shillong)
                        case 3654: // Nongstoin, Meghalaya (Shillong)
                        case 3655: // Khliehriat, Meghalaya (Shillong)
                        case 3656: // Mawkyrwat, Meghalaya (Shillong)
                        case 3657: // Mairang, Meghalaya (Shillong)
                        case 3658: // Williamnagar, Meghalaya (Shillong)
                        case 3659: // Resubelpara (Mendipathar), Meghalaya (Shillong)
                        case 3661: // Kokrajhar, Bongaigaon (Kokrajhar)
                        case 3662: // Dhubri, Bongaigaon (Kokrajhar)
                        case 3663: // Goalpara, Bongaigaon (Kokrajhar)
                        case 3664: // Hajo, Guwahati
                        case 3665: // Tarabarihat, Guwahati
                        case 3666: // Barpeta Road, Guwahati
                        case 3667: // Bilasipara, Bongaigaon (Kokrajhar)
                        case 3668: // Bijni, Bongaigaon (Kokrajhar)
                        case 3669: // Abhayapuri, Bongaigaon (Kokrajhar)
                        case 3670: // Maibong, Nagaon
                        case 3671: // Diphu, Nagaon
                        case 3672: // Nagaon, Nagaon
                        case 3673: // Haflong, Nagaon
                        case 3674: // Hojai, Nagaon
                        case 3675: // Bokajan, Nagaon
                        case 3676: // Howraghat, Nagaon
                        case 3677: // Baithalangshu, Nagaon
                        case 3678: // Morigaon, Nagaon
                        case 3711: // Udalguri, Tezpur
                        case 3712: // Tezpur, Tezpur
                        case 3713: // Mangaldoi, Tezpur
                        case 3714: // Rangapara, Tezpur
                        case 3715: // Gohpur, Tezpur
                        case 3751: // Digboi, Tinsukia (Dibrugarh)
                        case 3752: // North Lakhimpur, Tinsukia (Dibrugarh)
                        case 3753: // Dhemaji, Tinsukia (Dibrugarh)
                        case 3754: // Moranhat, Tinsukia (Dibrugarh)
                        case 3756: // Sadiya, Tinsukia (Dibrugarh)
                        case 3758: // Dhakuakhana, Tinsukia (Dibrugarh)
                        case 3759: // Bihupuria, Tinsukia (Dibrugarh)
                        case 3771: // Mariani, Jorhat
                        case 3772: // Sibsagar, Jorhat
                        case 3774: // Golaghat, Jorhat
                        case 3775: // Majuli, Jorhat
                        case 3776: // Bokakhat, Jorhat
                        case 3777: // Yangkiyang, Arunachal Pradesh (Zero)
                        case 3778: // Pakkekesang, Arunachal Pradesh (Zero)
                        case 3779: // Roing-Iii (Mariso), Arunachal Pradesh (Zero)
                        case 3780: // Dirang, Arunachal Pradesh (Zero)
                        case 3782: // Kalaktung (Bomdila), Arunachal Pradesh (Zero)
                        case 3783: // Along, Arunachal Pradesh (Zero)
                        case 3784: // Nefra, Arunachal Pradesh (Zero)
                        case 3785: // Bameng, Arunachal Pradesh (Zero)
                        case 3786: // Khonsa, Arunachal Pradesh (Zero)
                        case 3787: // Seppa, Arunachal Pradesh (Zero)
                        case 3788: // Kolaring, Arunachal Pradesh (Zero)
                        case 3789: // Huri, Arunachal Pradesh (Zero)
                        case 3790: // Tali, Arunachal Pradesh (Zero)
                        case 3791: // Taliha, Arunachal Pradesh (Zero)
                        case 3792: // Daporizo, Arunachal Pradesh (Zero)
                        case 3793: // Mechuka, Arunachal Pradesh (Zero
                        case 3794: // Tawang, Arunachal Pradesh (Zero)
                        case 3795: // Basar, Arunachal Pradesh (Zero)
                        case 3797: // Pangin, Arunachal Pradesh (Zero)
                        case 3798: // Mariyang, Arunachal Pradesh (Zero)
                        case 3799: // Tuting, Arunachal Pradesh (Zero)
                        case 3800: // Jairampur, Arunachal Pradesh (Zero)
                        case 3801: // Anini, Arunachal Pradesh (Zero)
                        case 3802: // Roing-Ii (Arda), Arunachal Pradesh (Zero)
                        case 3803: // Roing-I, Arunachal Pradesh (Zero)
                        case 3804: // Tezu, Arunachal Pradesh (Zero)
                        case 3805: // Hayuliang, Arunachal Pradesh (Zero)
                        case 3806: // Chowkhem, Arunachal Pradesh (Zero)
                        case 3807: // Miao, Arunachal Pradesh (Zero)
                        case 3808: // Changlang, Arunachal Pradesh (Zero)
                        case 3809: // Sagalee, Arunachal Pradesh (Zero)
                        case 3821: // R.K.Pur, Tripura (Agartala)
                        case 3822: // Dharam Nagar, Tripura (Agartala)
                        case 3823: // Belonia, Tripura (Agartala)
                        case 3824: // Kailsahar, Tripura (Agartala)
                        case 3825: // Khowai, Tripura (Agartala)
                        case 3826: // Ambasa, Tripura (Agartala)
                        case 3830: // Champai-Ii (Chiapui), Mizoram (Aizwal)
                        case 3831: // Champa-I, Mizoram (Aizwal)
                        case 3834: // Demagiri, Mizoram (Aizwal)
                        case 3835: // Saiha-I, Mizoram (Aizwal)
                        case 3836: // Saiha-Ii (Tuipang), Mizoram (Aizwal)
                        case 3837: // Kolasib, Mizoram (Aizwal)
                        case 3838: // Aizwal-Ii (Serchip), Mizoram (Aizwal)
                        case 3839: // Jalukie, Nagaland (Kohima)
                        case 3841: // Vdarbondh, Silchar
                        case 3842: // Silchar, Silchar
                        case 3843: // Karimganj, Silchar
                        case 3844: // Hailakandi, Silchar
                        case 3845: // Ukhrul Central, Manipur (Imphal)
                        case 3848: // Thonbal, Manipur (Imphal)
                        case 3860: // Wokha, Nagaland (Kohima)
                        case 3861: // Tuengsang, Nagaland (Kohima)
                        case 3862: // Dimapur, Nagaland (Kohima)
                        case 3863: // Kiphire, Nagaland (Kohima)
                        case 3865: // Phek, Nagaland (Kohima)
                        case 3867: // Zuenheboto, Nagaland (Kohima)
                        case 3869: // Mon, Nagaland (Kohima)
                        case 3870: // Ukhrursouth (Kassemkhulen), Manipur (Imphal)
                        case 3871: // Mao (Korang), Manipur (Imphal)
                        case 3872: // Chandel, Manipur (Imphal)
                        case 3873: // Thinghat, Manipur (Imphal)
                        case 3874: // Churchandpur, Manipur (Imphal)
                        case 3876: // Jiribam, Manipur (Imphal)
                        case 3877: // Tamenglong, Manipur (Imphal)
                        case 3878: // Chakpikarong, Manipur (Imphal)
                        case 3879: // Bishenpur, Manipur (Imphal)
                        case 3880: // Sadarhills (Kangpokai), Manipur (Imphal)
                        case 4111: // Sriperumpudur, Chengalpattu (Kancheepuram)
                        case 4112: // Kancheepuram, Chengalpattu (Kancheepuram)
                        case 4114: // Chengalpattu, Chengalpattu (Kancheepuram)
                        case 4115: // Madurantagam, Chengalpattu (Kancheepuram)
                        case 4116: // Tiruvellore, Chengalpattu (Kancheepuram)
                        case 4118: // Tiruttani, Chengalpattu (Kancheepuram)
                        case 4119: // Ponneri, Chengalpattu (Kancheepuram)
                        case 4142: // Cuddalore, Cuddalore
                        case 4143: // Virudhachalam, Cuddalore
                        case 4144: // Chidambaram, Cuddalore
                        case 4145: // Gingee, Cuddalore
                        case 4146: // Villupuram, Cuddalore
                        case 4147: // Tindivanam, Cuddalore
                        case 4149: // Ulundurpet, Cuddalore
                        case 4151: // Kallkurichi, Cuddalore
                        case 4153: // Arakandanallur, Cuddalore
                        case 4171: // Gudiyatham, Vellore
                        case 4172: // Ranipet, Vellore
                        case 4173: // Arni, Vellore
                        case 4174: // Vaniyambadi, Vellore
                        case 4175: // Tiruvannamalai, Vellore
                        case 4177: // Arkonam, Vellore
                        case 4179: // Tirupattur, Vellore
                        case 4181: // Polur, Vellore
                        case 4182: // Tiruvettipuram, Vellore
                        case 4183: // Wandiwash, Vellore
                        case 4188: // Chengam, Vellore
                        case 4202: // Mulanur, Erode
                        case 4204: // Kodumudi, Erode
                        case 4252: // Udumalpet, Coimbatore
                        case 4253: // Anamalai, Coimbatore
                        case 4254: // Mettupalayam, Coimbatore
                        case 4255: // Palladum, Coimbatore
                        case 4256: // Bhavani, Erode
                        case 4257: // Kangayam, Erode
                        case 4258: // Dharampuram, Erode
                        case 4259: // Pollachi, Coimbatore
                        case 4262: // Gudalur, Ooty
                        case 4266: // Kotagiri, Ooty
                        case 4268: // Velur, Salem
                        case 4281: // Yercaud, Salem
                        case 4282: // Attur, Salem
                        case 4283: // Sankagiri, Salem
                        case 4285: // Gobichettipalayam, Erode
                        case 4286: // Namakkal, Salem
                        case 4287: // Rasipuram, Salem
                        case 4288: // Tiruchengode, Salem
                        case 4290: // Omalur, Salem
                        case 4292: // Valapady, Salem
                        case 4294: // Perundurai, Erode
                        case 4295: // Sathiyamangalam, Erode
                        case 4296: // Avanashi, Coimbatore
                        case 4298: // Metturdam, Salem
                        case 4320: // Aravakurichi, Trichy
                        case 4322: // Pudukkottai, Trichy
                        case 4323: // Kulithalai, Trichy
                        case 4324: // Karur, Trichy
                        case 4326: // Musiri, Trichy
                        case 4327: // Thuraiyure, Trichy
                        case 4328: // Perambalur, Trichy
                        case 4329: // Ariyalur, Trichy
                        case 4331: // Jayamkondan, Trichy
                        case 4332: // Manaparai, Trichy
                        case 4333: // Ponnamaravathi, Trichy
                        case 4339: // Keeranur, Trichy
                        case 4341: // Uthangarai, Dharmapuri
                        case 4342: // Dharmapuri, Dharmapuri
                        case 4343: // Krishnagiri, Dharmapuri
                        case 4344: // Hosur, Dharmapuri
                        case 4346: // Harur, Dharmapuri
                        case 4347: // Denkanikoitah, Dharmapuri
                        case 4348: // Palacode, Dharmapuri
                        case 4362: // Thanjavur, Thanjavur
                        case 4364: // Mayiladuthurai, Thanjavur
                        case 4365: // Nagapattinam, Thanjavur
                        case 4366: // Tiruvarur, Thanjavur
                        case 4367: // Mannargudi, Thanjavur
                        case 4368: // Karaikal, Thanjavur
                        case 4369: // Thiruraipoondi, Thanjavur
                        case 4371: // Arantangi, Trichy
                        case 4372: // Orathanad, Thanjavur
                        case 4373: // Pattukottai, Thanjavur
                        case 4374: // Papanasam, Thanjavur
                        case 4542: // Kodaikanal, Madurai
                        case 4543: // Batlagundu, Madurai
                        case 4544: // Natham, Madurai
                        case 4545: // Palani, Madurai
                        case 4546: // Theni, Madurai
                        case 4549: // Thirumanglam, Madurai
                        case 4551: // Vedasandur, Madurai
                        case 4552: // Usiliampatti, Madurai
                        case 4553: // Oddanchatram, Madurai
                        case 4554: // Cumbum, Madurai
                        case 4561: // Devakottai, Karaikudi
                        case 4562: // Virudhunagar, Virudhunagar
                        case 4563: // Rajapalayam, Virudhunagar
                        case 4564: // Paramakudi, Karaikudi
                        case 4565: // Karaikudi, Karaikudi
                        case 4566: // Aruppukottai, Virudhunagar
                        case 4567: // Ramanathpuram, Karaikudi
                        case 4573: // Rameshwaram, Karaikudi
                        case 4574: // Manamadurai, Karaikudi
                        case 4575: // Sivaganga, Karaikudi
                        case 4576: // Mudukulathur, Karaikudi
                        case 4577: // Tirupathur, Karaikudi
                        case 4630: // Srivaikundam, Tuticorin
                        case 4632: // Kovilpatti, Tuticorin
                        case 4633: // Tenkasi, Tirunelvelli
                        case 4634: // Ambasamudram, Tirunelvelli
                        case 4635: // Nanguneri, Tirunelvelli
                        case 4636: // Sankaran Koil, Tirunelvelli
                        case 4637: // Valliyoor, Tirunelvelli
                        case 4638: // Vilathikulam, Tuticorin
                        case 4639: // Tiruchendur, Tuticorin
                        case 4651: // Kuzhithurai, Nagarcoil
                        case 4652: // Nagercoil, Nagarcoil
                        // Dup of 472? case 4728: // Nedumandad, Thiruvananthapuram
                        case 4733: // Pathanamthitta, Tiruvalla
                        case 4734: // Adoor, Tiruvalla
                        case 4735: // Ranni, Tiruvalla
                        case 4822: // Palai, Kottayam
                        case 4828: // Kanjirapally, Kottayam
                        case 4829: // Vaikom, Kottayam
                        case 4862: // Thodupuzha, Ernakulam
                        case 4864: // Adimaly, Ernakulam
                        case 4865: // Munnar, Ernakulam
                        case 4868: // Nedumgandam, Ernakulam
                        case 4869: // Peermedu, Ernakulam
                        case 4884: // Vadakkanchery, Trichur
                        case 4885: // Kunnamkulam, Trichur
                        case 4890: // Bitra, Kavarathy
                        case 4891: // Amini, Kavarathy
                        case 4892: // Minicoy, Kavarathy
                        case 4893: // Androth, Kavarathy
                        case 4894: // Agathy, Kavarathy
                        case 4895: // Kalpeni, Kavarathy
                        case 4896: // Kavarathy, Kavarathy
                        case 4897: // Kadamath, Kavarathy
                        case 4898: // Kiltan, Kavarathy
                        case 4899: // Chetlat, Kavarathy
                        case 4922: // Alathur, Palghat
                        case 4923: // Koduvayur, Palghat
                        case 4924: // Mannarghat, Palghat
                        case 4926: // Shoranur, Palghat
                        case 4931: // Nilambur, Calicut (Kozhikode)
                        case 4933: // Perinthalmanna, Calicut (Kozhikode)
                        case 4935: // Mananthody, Calicut (Kozhikode)
                        case 4936: // Kalpetta, Calicut (Kozhikode)
                        case 4982: // Taliparamba, Cannanore
                        case 4985: // Payyanur, Cannanore
                        case 4994: // Kasargode, Cannanore
                        case 4997: // Kanhangad, Cannanore
                        case 4998: // Uppala, Cannanore
                        case 5111: // Akbarpur, Kanpur
                        case 5112: // Bilhaur, Kanpur
                        case 5113: // Bhognipur (Pakhrayan), Kanpur
                        case 5114: // Derapur (Jhinjak), Kanpur
                        case 5115: // Ghatampur, Kanpur
                        case 5142: // Purwa (Bighapur), Unnao
                        case 5143: // Hasanganj, Unnao
                        case 5144: // Safipur, Unnao
                        case 5162: // Orai, Orai
                        case 5164: // Kalpi, Orai
                        case 5165: // Konch, Orai
                        case 5168: // Jalaun, Orai
                        case 5170: // Chirgaon (Moth), Jhansi
                        case 5171: // Garauth, Jhansi
                        case 5172: // Mehraun, Jhansi
                        case 5174: // Jhansi, Jhansi
                        case 5175: // Lalitpur-Ii (Talbehat), Jhansi
                        case 5176: // Lalitpur-I (Lalitpur), Jhansi
                        case 5178: // Mauranipur, Jhansi
                        case 5180: // Fateh-Pur-I (Fatehpur), Fatehpur
                        case 5181: // Bindki, Fatehpur
                        case 5182: // Khaga, Fatehpur
                        case 5183: // Fatehpur-Ii (Gazipur), Fatehpur
                        case 5190: // Baberu, Banda
                        case 5191: // Naraini (Attarra), Banda
                        case 5192: // Banda, Banda
                        case 5194: // Karvi-Ii (Manikpur), Banda
                        case 5195: // Mau (Rajapur), Banda
                        case 5198: // Karvi -I (Karvi), Banda
                        case 5212: // Malihabad, Lucknow
                        case 5240: // Fatehpur, Barabanki
                        case 5241: // Ramsanehi Ghat, Barabanki
                        case 5244: // Haidergarh, Barabanki
                        case 5248: // Barabanki, Barabanki
                        case 5250: // Bahraich-Ii (Bhinga), Bahraich
                        case 5251: // Kaisarganj-I (Kaiserganj), Bahraich
                        case 5252: // Bahraich-I (Bahrailh), Bahraich
                        case 5253: // Nanpara-I (Nanpara), Bahraich
                        case 5254: // Nanparah-Ii (Mihinpurwa), Bahraich
                        case 5255: // Kaisarganh-Ii (Mahasi), Bahraich
                        case 5260: // Tarabganj-I (Terabganj), Gonda
                        case 5261: // Tarabganj-Ii (Colonelganj), Gonda
                        case 5262: // Gonda, Gonda
                        case 5263: // Balarampur-I (Balrampur), Gonda
                        case 5264: // Balarampur-Ii (Tulsipur), Gonda
                        case 5265: // Utraula, Gonda
                        case 5270: // Bikapur, Faizabad
                        case 5271: // Akbarpur-I (Akbarpur), Faizabad
                        case 5273: // Tandai-I (Tanda), Faizabad
                        case 5274: // Tanda-Ii (Baskhari), Faizabad
                        case 5275: // Akbarpur-Ii (Jalalpur), Faizabad
                        case 5278: // aizabad, Faizabad
                        case 5280: // Rath, Hamirpur
                        case 5281: // Mahoba, Hamirpur
                        case 5282: // Hamirpur, Hamirpur
                        case 5283: // Charkhari, Hamirpur
                        case 5284: // Maudaha, Hamirpur
                        case 5311: // Salon -I (Salon), Raibareilly
                        case 5313: // Salon-Ii (Jais), Raibareilly
                        case 5315: // Dalmau-Ii (Lalganj), Raibareilly
                        case 5317: // Dalmau-I (Dalmau), Raibareilly
                        case 5331: // Bharwari, Allahabad
                        case 5332: // Phoolpur, Allahabad
                        case 5333: // Karchhana (Shankergarh), Allahabad
                        case 5334: // Meja (Sirsa), Allahabad
                        case 5335: // Soraon, Allahabad
                        case 5341: // Kunda, Pratapgarh
                        case 5342: // Pratapgarh, Pratapgarh
                        case 5343: // Patti, Pratapgarh
                        case 5361: // Musafirkhana, Sultanpur
                        case 5362: // Sultanpur, Sultanpur
                        case 5364: // Kadipur, Sultanpur
                        case 5368: // Amethi, Sultanpur
                        case 5412: // Chandauli (Mugalsarai), Varansi
                        case 5413: // Chakia, Varansi
                        case 5414: // Bhadohi, Varansi
                        case 5440: // Mirzapur-Ii (Hallia), Mirzapur
                        case 5442: // Mirzapur-I (Mirzapur), Mirzapur
                        case 5443: // Chunur, Mirzapur
                        case 5444: // Robertsganj-I, Mirzapur
                        case 5445: // Robertsganj -Ii (Obra), Mirzapur
                        case 5446: // Dudhi-Ii (Pipri), Mirzapur
                        case 5447: // Dudhi-I (Dudhi), Mirzapur
                        case 5450: // Kerakat, Jaunpur
                        case 5451: // Mariyahu, Jaunpur
                        case 5452: // Jaunpur, Jaunpur
                        case 5453: // Shahganj, Jaunpur
                        case 5454: // Machlishahar, Jaunpur
                        case 5460: // Phulpur-I (Phulpur), Azamgarh
                        case 5461: // Ghosi, Azamgarh
                        case 5462: // Azamgarh, Azamgarh
                        case 5463: // Lalganj, Azamgarh
                        case 5464: // Maunathbhanjan, Azamgarh
                        case 5465: // Phulpur-Ii (Atrawlia), Azamgarh
                        case 5466: // Sagri, Azamgarh
                        case 5491: // Rasara, Ballia
                        case 5493: // Mohamdabad, Ghazipur
                        case 5494: // Bansdeeh, Ballia
                        case 5495: // Saidpur, Ghazipur
                        case 5496: // Ballia-Ii (Raniganj), Ballia
                        case 5497: // Zamania, Ghazipur
                        case 5498: // Ballia-I (Ballia), Ballia
                        case 5521: // Bansgaon-Ii (Barhal Ganj), Gorakhpur
                        case 5522: // Pharenda-I (Compierganj), Gorakhpur
                        case 5523: // Maharajganj, Gorakhpur
                        case 5524: // Pharenda-Ii (Anand Nagar), Gorakhpur
                        case 5525: // Bansgaon -I (Bansgaon), Gorakhpur
                        case 5541: // Domariyaganj, Basti
                        case 5542: // Basti, Basti
                        case 5543: // Naugarh-Ii (Barhani), Basti
                        case 5544: // Naugarh-I (Tetribazar), Basti
                        case 5545: // Bansi, Basti
                        case 5546: // Harraiya, Basti
                        case 5547: // Khalilabad -I, Basti
                        case 5548: // Khalilabad-Ii (Mehdawal), Basti
                        case 5561: // Salempur-Ii (Barhaj), Deoria
                        case 5563: // Captanganj (Khadda), Deoria
                        case 5564: // Padrauna, Deoria
                        case 5566: // Salempur-I (Salempur), Deoria
                        case 5567: // Captanganj-I (Captanganj), Deoria
                        case 5568: // Deoria, Deoria
                        case 5612: // Ferozabad, Agra
                        case 5613: // Achhnera, Agra
                        case 5614: // Jarar, Agra
                        case 5640: // Kaman, Bharatpur
                        case 5641: // Deeg, Bharatpur
                        case 5642: // Dholpur, Bharatpur
                        case 5643: // Nadbai, Bharatpur
                        case 5644: // Bharatpur, Bharatpur
                        case 5645: // Rupbas, Bharatpur
                        case 5646: // Baseri, Bharatpur
                        case 5647: // Bari, Bharatpur
                        case 5648: // Bayana, Bharatpur
                        case 5661: // Sadabad, Mathura
                        case 5662: // Chhata (Kosikalan), Mathura
                        case 5663: // Mant (Vrindavan), Mathura
                        case 5664: // Mant (Vrindavan), Mathura
                        case 5671: // Jasrana, Mainpuri
                        case 5672: // Mainpuri, Mainpuri
                        case 5673: // Bhogaon, Mainpuri
                        case 5676: // Shikohabad, Mainpuri
                        case 5677: // Karhal, Mainpuri
                        case 5680: // Bharthana, Etawah
                        case 5681: // Bidhuna, Etawah
                        case 5683: // Auraiya, Etawah
                        case 5688: // Etawah, Etawah
                        case 5690: // Kaimganj, Farrukhabad
                        case 5691: // Chhibramau, Farrukhabad
                        case 5692: // Farrukhabad (Fategarh), Farrukhabad
                        case 5694: // Kannauj, Farrukhabad
                        case 5721: // Sikandra Rao, Aligarh
                        case 5722: // Hathras, Aligarh
                        case 5723: // Atrauli, Aligarh
                        case 5724: // Khair, Aligarh
                        case 5731: // Garhmukteshwar, Ghaziabad
                        case 5732: // Bulandshahr, Ghaziabad
                        case 5733: // Pahasu, Ghaziabad
                        case 5734: // Debai, Ghaziabad
                        case 5735: // Sikandrabad, Ghaziabad
                        case 5736: // Siyana, Ghaziabad
                        case 5738: // Khurja, Ghaziabad
                        case 5740: // Aliganj (Ganjdundwara), Etah
                        case 5742: // Etah, Etah
                        case 5744: // Kasganj, Etah
                        case 5745: // Jalesar, Etah
                        case 5821: // Pitamberpur, Bareilly
                        case 5822: // Baheri, Bareilly
                        case 5823: // Aonla -I, Bareilly
                        case 5824: // Aonla-Ii (Ramnagar), Bareilly
                        case 5825: // Nawabganj, Bareilly
                        case 5831: // Dataganj, Badaun
                        case 5832: // Badaun, Badaun
                        case 5833: // Sahaswan, Badaun
                        case 5834: // Bisauli, Badaun
                        case 5836: // Gunnaur, Badaun
                        case 5841: // Tilhar, Sahjahanpur
                        case 5842: // Shahjahanpur, Sahjahanpur
                        case 5843: // Jalalabad, Sahjahanpur
                        case 5844: // Powayan, Sahjahanpur
                        case 5850: // Hardoi-Ii (Baghavli), Hardoi
                        case 5851: // Bilgam-I (Madhoganj), Hardoi
                        case 5852: // Hardoi-I (Hardoi), Hardoi
                        case 5853: // Shahabad, Hardoi
                        case 5854: // Sandila, Hardoi
                        case 5855: // Bilgram-Ii (Sandi), Hardoi
                        case 5861: // Misrikh-Ii (Aurangabad), Sitapur
                        case 5862: // Sitapur, Sitapur
                        case 5863: // Biswan, Sitapur
                        case 5864: // Sidhauli (Mahmodabad), Sitapur
                        case 5865: // Misrikh -I (Misrikh), Sitapur
                        case 5870: // Kheri-Ii (Bhira), Lakhimpur Kheri
                        case 5871: // Nighasan-I (Palliakalan), Lakhimpur Kheri
                        case 5872: // Kheri-I (Kheri), Lakhimpur Kheri
                        case 5873: // Nighasan-Ii (Tikonia), Lakhimpur Kheri
                        case 5874: // Nighasan-Iii (Dhaurahra), Lakhimpur Kheri
                        case 5875: // Mohamdi-Ii (Maigalganj), Lakhimpur Kheri
                        case 5876: // Mohamdi-I (Mohamdi), Lakhimpur Kheri
                        case 5880: // Puranpur, Pilibhit
                        case 5881: // Bisalpur, Pilibhit
                        case 5882: // Pilibhit, Pilibhit
                        case 5921: // Bilari, Moradabad
                        case 5922: // Amroha, Moradabad
                        case 5923: // Sambhal, Moradabad
                        case 5924: // Hasanpur, Moradabad
                        case 5942: // Nainital, Nainital
                        case 5943: // Khatima, Nainital
                        case 5944: // Kichha-I (Rudrapur), Nainital
                        case 5945: // Haldwani-Ii (Chorgalian), Nainital
                        case 5946: // Haldwani-I, Nainital
                        case 5947: // Kashipur, Nainital
                        case 5948: // Khatima-Ii (Sitarganj), Nainital
                        case 5949: // Kichha-Ii (Bazpur), Nainital
                        case 5960: // Shahabad, Rampur
                        case 5961: // Munsiari, Almora
                        case 5962: // Almora, Almora
                        case 5963: // Bageshwar, Almora
                        case 5964: // Pithoragarh, Almora
                        case 5965: // Champawat, Almora
                        case 5966: // Ranikhet, Almora
                        case 5967: // Dharchula, Almora
                        case 6111: // Hilsa, Patna
                        case 6112: // Biharsharif, Patna
                        case 6114: // Jahanabad, Gaya
                        case 6115: // Danapur, Patna
                        case 6132: // Barh, Patna
                        case 6135: // Bikram, Patna
                        case 6150: // Hathua, Chapra
                        case 6151: // Sidhawalia, Chapra
                        case 6152: // Chapra, Chapra
                        case 6153: // Maharajganj, Chapra
                        case 6154: // Siwan, Chapra
                        case 6155: // Ekma, Chapra
                        case 6156: // Gopalganj, Chapra
                        case 6157: // Mairwa, Chapra
                        case 6158: // Sonepur, Chapra
                        case 6159: // Masrakh, Chapra
                        case 6180: // Adhaura, Sasaram
                        case 6181: // Piro, Arrah
                        case 6182: // Arrah, Arrah
                        case 6183: // Buxar, Arrah
                        case 6184: // Sasaram, Sasaram
                        case 6185: // Bikramganj, Sasaram
                        case 6186: // Aurangabad, Gaya
                        case 6187: // Mohania, Sasaram
                        case 6188: // Rohtas, Sasaram
                        case 6189: // Bhabhua, Sasaram
                        case 6222: // Sheohar, Muzaffarpur
                        case 6223: // Motipur, Muzaffarpur
                        case 6224: // Hajipur, Muzaffarpur
                        case 6226: // Sitamarhi, Muzaffarpur
                        case 6227: // Mahua, Muzaffarpur
                        case 6228: // Pupri, Muzaffarpur
                        case 6229: // Bidupur, Muzaffarpur
                        case 6242: // Benipur, Darbhanga
                        case 6243: // Begusarai, Darbhanga
                        case 6244: // Khagaria, Darbhanga
                        case 6245: // Gogri, Darbhanga
                        case 6246: // Jainagar, Darbhanga
                        case 6247: // Singhwara, Darbhanga
                        case 6250: // Dhaka, Motihari
                        case 6251: // Bagaha, Motihari
                        case 6252: // Motihari, Motihari
                        case 6253: // Narkatiaganj, Motihari
                        case 6254: // Bettiah, Motihari
                        case 6255: // Raxaul, Motihari
                        case 6256: // Ramnagar, Motihari
                        case 6257: // Barachakia, Motihari
                        case 6258: // Areraj, Motihari
                        case 6259: // Pakridayal, Motihari
                        case 6271: // Benipatti, Darbhanga
                        case 6272: // Darbhanga, Darbhanga
                        case 6273: // Jhajharpur, Darbhanga
                        case 6274: // Samastipur, Darbhanga
                        case 6275: // Rosera, Darbhanga
                        case 6276: // Madhubani, Darbhanga
                        case 6277: // Phulparas, Darbhanga
                        case 6278: // Dalsinghsarai, Darbhanga
                        case 6279: // Barauni, Darbhanga
                        case 6322: // Wazirganj, Gaya
                        case 6323: // Dumraon, Arrah
                        case 6324: // Nawada, Gaya
                        case 6325: // Pakribarwan, Gaya
                        case 6326: // Sherghati, Gaya
                        case 6327: // Rafiganj, Gaya
                        case 6328: // Daudnagar, Gaya
                        case 6331: // Imamganj, Gaya
                        case 6332: // Nabinagar, Gaya
                        case 6336: // Rajauli, Gaya
                        case 6337: // Arwal, Gaya
                        case 6341: // Seikhpura, Monghyr
                        case 6342: // H.Kharagpur, Monghyr
                        case 6344: // Monghyr, Monghyr
                        case 6345: // Jamui, Monghyr
                        case 6346: // Lakhisarai, Monghyr
                        case 6347: // Chakai, Monghyr
                        case 6348: // Mallehpur, Monghyr
                        case 6349: // Jhajha, Monghyr
                        case 6420: // Amarpur, Bhagalpur
                        case 6421: // Naugachia, Bhagalpur
                        case 6422: // Godda, Deoghar (Dumka)
                        case 6423: // Maheshpur Raj, Deoghar (Dumka)
                        case 6424: // Banka, Bhagalpur
                        case 6425: // Katoria, Bhagalpur
                        case 6426: // Rajmahal, Deoghar (Dumka)
                        case 6427: // Kathikund, Deoghar (Dumka)
                        case 6428: // Nala, Deoghar (Dumka)
                        case 6429: // Kahalgaon, Bhagalpur
                        case 6431: // Jharmundi, Deoghar (Dumka)
                        case 6432: // Deoghar, Deoghar (Dumka)
                        case 6433: // Jamtara, Deoghar (Dumka)
                        case 6434: // Dumka, Deoghar (Dumka)
                        case 6435: // Pakur, Deoghar (Dumka)
                        case 6436: // Sahibganj, Deoghar (Dumka)
                        case 6437: // Mahagama, Deoghar (Dumka)
                        case 6438: // Madhupur, Deoghar (Dumka)
                        case 6451: // Barsoi, Katihar
                        case 6452: // Katihar, Katihar
                        case 6453: // Araria, Katihar
                        case 6454: // Purnea, Katihar
                        case 6455: // Forbesganj, Katihar
                        case 6457: // Korha, Katihar
                        case 6459: // Thakurganj, Katihar
                        case 6461: // Raniganj, Katihar
                        case 6462: // Dhamdaha, Katihar
                        case 6466: // Kishanganj, Katihar
                        case 6467: // Banmankhi, Katihar
                        case 6471: // Birpur, Saharsa
                        case 6473: // Supaul, Saharsa
                        case 6475: // S.Bakhtiarpur, Saharsa
                        case 6476: // Madhepura, Saharsa
                        case 6477: // Triveniganj, Saharsa
                        case 6478: // Saharsa, Saharsa
                        case 6479: // Udakishanganj, Saharsa
                        case 6522: // Muri, Ranchi
                        case 6523: // Ghaghra, Ranchi
                        case 6524: // Gumla, Ranchi
                        case 6525: // Simdega, Ranchi
                        case 6526: // Lohardaga, Ranchi
                        case 6527: // Kolebira, Ranchi
                        case 6528: // Khunti, Ranchi
                        case 6529: // Itki, Ranchi
                        case 6530: // Bundu, Ranchi
                        case 6531: // Mandar, Ranchi
                        case 6532: // Giridih, Hazaribagh
                        case 6533: // Basia, Ranchi
                        case 6534: // Jhumaritalaiya, Hazaribagh
                        case 6535: // Chainpur, Ranchi
                        case 6536: // Palkot, Ranchi
                        case 6538: // Torpa, Ranchi
                        case 6539: // Bolwa, Ranchi
                        case 6540: // Govindpur, Dhanbad
                        case 6541: // Chatra, Hazaribagh
                        case 6542: // Bokaro, Dhanbad
                        case 6543: // Barhi, Hazaribagh
                        case 6544: // Gomia, Dhanbad
                        case 6545: // Mandu, Hazaribagh
                        case 6546: // Hazaribagh, Hazaribagh
                        case 6547: // Chavparan, Hazaribagh
                        case 6548: // Ichak, Hazaribagh
                        case 6549: // Bermo, Dhanbad
                        case 6550: // Hunterganj, Hazaribagh
                        case 6551: // Barkagaon, Hazaribagh
                        case 6553: // Ramgarh, Hazaribagh
                        case 6554: // Rajdhanwar, Hazaribagh
                        case 6556: // Tisri, Hazaribagh
                        case 6557: // Bagodar, Hazaribagh
                        case 6558: // Dumri(Isribazar), Hazaribagh
                        case 6559: // Simaria, Hazaribagh
                        case 6560: // Patan, Daltonganj
                        case 6561: // Garhwa, Daltonganj
                        case 6562: // Daltonganj, Daltonganj
                        case 6563: // Bhawanathpur, Daltonganj
                        case 6564: // Nagarutari, Daltonganj
                        case 6565: // Latehar, Daltonganj
                        case 6566: // Japla, Daltonganj
                        case 6567: // Barwadih, Daltonganj
                        case 6568: // Balumath, Daltonganj
                        case 6569: // Garu, Daltonganj
                        case 6581: // Bhandaria, Daltonganj
                        case 6582: // Chaibasa, Jamshedpur
                        case 6583: // Kharsawa, Jamshedpur
                        case 6584: // Bishrampur, Daltonganj
                        case 6585: // Ghatsila, Jamshedpur
                        case 6586: // Chainpur, Daltonganj
                        case 6587: // Chakardharpur, Jamshedpur
                        case 6588: // Jagarnathpur, Jamshedpur
                        case 6589: // Jhinkpani, Jamshedpur
                        case 6591: // Chandil, Jamshedpur
                        case 6593: // Manoharpur, Jamshedpur
                        case 6594: // Baharagora, Jamshedpur
                        case 6596: // Noamundi, Jamshedpur
                        case 6597: // Saraikela (Adstyapur), Jamshedpur
                        case 6621: // Hemgiri, Sundargarh (Rourkela)
                        case 6622: // Sundargarh, Sundargarh (Rourkela)
                        case 6624: // Rajgangpur, Sundargarh (Rourkela)
                        case 6625: // Lahunipara, Sundargarh (Rourkela)
                        case 6626: // Banaigarh, Sundargarh (Rourkela)
                        case 6640: // Bagdihi, Sambalpur
                        case 6641: // Deodgarh, Sambalpur
                        case 6642: // Kuchinda, Sambalpur
                        case 6643: // Barkot, Sambalpur
                        case 6644: // Rairakhol, Sambalpur
                        case 6645: // Jharsuguda, Sambalpur
                        case 6646: // Bargarh, Sambalpur
                        case 6647: // Naktideul, Sambalpur
                        case 6648: // Patnagarh, Balangir
                        case 6649: // Jamankira, Sambalpur
                        case 6651: // Birmaharajpur, Balangir
                        case 6652: // Balangir, Balangir
                        case 6653: // Dunguripali, Balangir
                        case 6654: // Sonapur, Balangir
                        case 6655: // Titlagarh, Balangir
                        case 6657: // Kantabhanji, Balangir
                        case 6670: // Bhawanipatna, Bhawanipatna
                        case 6671: // Rajkhariar, Bhawanipatna
                        case 6672: // Dharamgarh, Bhawanipatna
                        case 6673: // Jayapatna, Bhawanipatna
                        case 6675: // T.Rampur, Bhawanipatna
                        case 6676: // M.Rampur, Bhawanipatna
                        case 6677: // Narlaroad, Bhawanipatna
                        case 6678: // Nowparatan, Bhawanipatna
                        case 6679: // Komana, Bhawanipatna
                        case 6681: // Jujumura, Sambalpur
                        case 6682: // Attabira, Sambalpur
                        case 6683: // Padmapur, Sambalpur
                        case 6684: // Paikamal, Sambalpur
                        case 6685: // Sohela, Sambalpur
                        case 6721: // Narsinghpur, Cuttack
                        case 6722: // Pardip, Cuttack
                        case 6723: // Athgarh, Cuttack
                        case 6724: // Jagatsinghpur, Cuttack
                        case 6725: // Dhanmandal, Cuttack
                        case 6726: // Jajapur Road, Cuttack
                        case 6727: // Kendrapara, Cuttack
                        case 6728: // Jajapur Town, Cuttack
                        case 6729: // Pattamundai, Cuttack
                        case 6731: // Anandapur, Dhenkanal
                        case 6732: // Hindol, Dhenkanal
                        case 6733: // Ghatgaon, Dhenkanal
                        case 6735: // Telkoi, Dhenkanal
                        case 6752: // Puri, Bhubaneswar (Puri)
                        case 6753: // Nayagarh, Bhubaneswar (Puri)
                        case 6755: // Khurda, Bhubaneswar (Puri)
                        case 6756: // Balugaon, Bhubaneswar (Puri)
                        case 6757: // Daspalla, Bhubaneswar (Puri)
                        case 6758: // Nimapara, Bhubaneswar (Puri)
                        case 6760: // Talcher, Dhenkanal
                        case 6761: // Chhendipada, Dhenkanal
                        case 6762: // Dhenkanal, Dhenkanal
                        case 6763: // Athmallik, Dhenkanal
                        case 6764: // Anugul, Dhenkanal
                        case 6765: // Palla Hara, Dhenkanal
                        case 6766: // Keonjhar, Dhenkanal
                        case 6767: // Barbil, Dhenkanal
                        case 6768: // Parajang, Dhenkanal
                        case 6769: // Kamakhyanagar, Dhenkanal
                        case 6781: // Basta, Balasore
                        case 6782: // Balasore, Balasore
                        case 6784: // Bhadrak, Balasore
                        case 6786: // Chandbali, Balasore
                        case 6788: // Soro, Balasore
                        case 6791: // Bangiriposi, Baripada
                        case 6792: // Baripada, Baripada
                        case 6793: // Betanati, Baripada
                        case 6794: // Rairangpur, Baripada
                        case 6795: // Udala, Baripada
                        case 6796: // Karanjia, Baripada
                        case 6797: // Jashipur, Baripada
                        case 6810: // Khalikote, Berhampur
                        case 6811: // Chhatrapur, Berhampur
                        case 6814: // Digapahandi, Berhampur
                        case 6815: // Parlakhemundi, Berhampur
                        case 6816: // Mohana, Berhampur
                        case 6817: // R.Udayigiri, Berhampur
                        case 6818: // Buguda, Berhampur
                        case 6819: // Surada, Berhampur
                        case 6821: // Bhanjanagar, Berhampur
                        case 6822: // Aska, Berhampur
                        case 6840: // Tumudibandha, Phulbani
                        case 6841: // Boudh, Phulbani
                        case 6842: // Phulbani, Phulbani
                        case 6843: // Puruna Katak, Phulbani
                        case 6844: // Kantamal, Phulbani
                        case 6845: // Phiringia, Phulbani
                        case 6846: // Baliguda, Phulbani
                        case 6847: // G.Udayagiri, Phulbani
                        case 6848: // Kotagarh, Phulbani
                        case 6849: // Daringbadi, Phulbani
                        case 6850: // Kalimela, Koraput
                        case 6852: // Koraput, Koraput
                        case 6853: // Sunabeda, Koraput
                        case 6854: // Jeypore, Koraput
                        case 6855: // Laxmipur, Koraput
                        case 6856: // Rayagada, Koraput
                        case 6857: // Gunupur, Koraput
                        case 6858: // Nowrangapur, Koraput
                        case 6859: // Motu, Koraput
                        case 6860: // Boriguma, Koraput
                        case 6861: // Malkangiri, Koraput
                        case 6862: // Gudari, Koraput
                        case 6863: // Bisam Cuttack, Koraput
                        case 6864: // Mathili, Koraput
                        case 6865: // Kashipur, Koraput
                        case 6866: // Umerkote, Koraput
                        case 6867: // Jharigan, Koraput
                        case 6868: // Nandapur, Koraput
                        case 6869: // Papadhandi, Koraput
                            parse_out->parse_state = STATE_INDIA_SUB_1_2_4;
                            parse_out->field_pos = 0;
                            parse_out->service_type = TSERV_LAND;
                            break;
                        // The 7000-9999 range (7*, 8*, and 9*) can be replaced with the AAAA-SSSSSS format without listing every
                        // allocation below.
                        case 7100: // Kuhi, Nagpur
                        case 7102: // Parseoni, Nagpur
                        case 7103: // Butibori, Nagpur
                        case 7104: // Hingua, Nagpur
                        case 7105: // Narkhed, Nagpur
                        case 7106: // Bhiwapur, Nagpur
                        case 7109: // Kamptee, Nagpur
                        case 7112: // Katol, Nagpur
                        case 7113: // Saoner, Nagpur
                        case 7114: // Ramtek, Nagpur
                        case 7115: // Mouda, Nagpur
                        case 7116: // Umrer, Nagpur
                        case 7118: // Kalmeshwar, Nagpur
                        case 7131: // Sironcha, Gadchiroli
                        case 7132: // Gadchiroli, Gadchiroli
                        case 7133: // Aheri, Gadchiroli
                        case 7134: // Bhamregadh, Gadchiroli
                        case 7135: // Chamorshi, Gadchiroli
                        case 7136: // Etapalli, Gadchiroli
                        case 7137: // Desaiganj, Gadchiroli
                        case 7138: // Dhanora, Gadchiroli
                        case 7139: // Kurkheda, Gadchiroli
                        case 7141: // Betul, Betul
                        case 7142: // Bhimpur, Betul
                        case 7143: // Bhainsdehi, Betul
                        case 7144: // Atner, Betul
                        case 7145: // Chicholi, Betul
                        case 7146: // Ghorandogri, Betul
                        case 7147: // Multai, Betul
                        case 7148: // Prabha Pattan, Betul
                        case 7149: // Tamia, Chhindwara
                        case 7151: // Samudrapur, Wardha
                        case 7152: // Wardha, Wardha
                        case 7153: // Hinganghat, Wardha
                        case 7155: // Seloo, Wardha
                        case 7156: // Talegaokarangal, Wardha
                        case 7157: // Arvi, Wardha
                        case 7158: // Deoli, Wardha
                        case 7160: // Jamai, Chhindwara
                        case 7161: // Parasia, Chhindwara
                        case 7162: // Chhindwara, Chhindwara
                        case 7164: // Pandhurna, Chhindwara
                        case 7165: // Saunsar, Chhindwara
                        case 7166: // Chaurai, Chhindwara
                        case 7167: // Amarwada, Chhindwara
                        case 7168: // Harrai, Chhindwara
                        case 7169: // Batkakhapa, Chhindwara
                        case 7170: // Chumur, Chandrapur
                        case 7171: // Gond Pipri, Chandrapur
                        case 7172: // Chandrapur, Chandrapur
                        case 7173: // Rajura, Chandrapur
                        case 7174: // Mul, Chandrapur
                        case 7175: // Bhadrawati, Chandrapur
                        case 7176: // Warora, Chandrapur
                        case 7177: // Brahmapuri, Chandrapur
                        case 7178: // Sinderwahi, Chandrapur
                        case 7179: // Nagbhir, Chandrapur
                        case 7180: // Salekasa, Bhandara
                        case 7181: // Lakhandur, Bhandara
                        case 7182: // Gondia, Bhandara
                        case 7183: // Tumsar, Bhandara
                        case 7184: // Bhandara, Bhandara
                        case 7185: // Pauni, Bhandara
                        case 7186: // Sakoli, Bhandara
                        case 7187: // Goregaon, Bhandara
                        case 7189: // Amagaon, Bhandara
                        case 7196: // Arjuni-Merogaon, Bhandara
                        case 7197: // Mohadi, Bhandara
                        case 7198: // Tirora, Bhandara
                        case 7199: // Deori, Bhandara
                        case 7201: // Kalamb, Yeotmal
                        case 7202: // Ralegaon, Yeotmal
                        case 7203: // Babhulgaon, Yeotmal
                        case 7220: // Chhikaldara, Amravati
                        case 7221: // Nandgaon, Amravati
                        case 7222: // Chandurrly, Amravati
                        case 7223: // Achalpur, Amravati
                        case 7224: // Daryapur, Amravati
                        case 7225: // Tiwasa, Amravati
                        case 7226: // Dharani, Amravati
                        case 7227: // Chandurbazar, Amravati
                        case 7228: // Morshi, Amravati
                        case 7229: // Warlydwarud, Amravati
                        case 7230: // Ghatanji, Yeotmal
                        case 7231: // Umarkhed, Yeotmal
                        case 7232: // Yeotmal, Yeotmal
                        case 7233: // Pusad, Yeotmal
                        case 7234: // Digras, Yeotmal
                        case 7235: // Pandharkawada, Yeotmal
                        case 7236: // Maregaon, Yeotmal
                        case 7237: // Marigaon, Yeotmal
                        case 7238: // Darwaha, Yeotmal
                        case 7239: // Wani, Yeotmal
                        case 7251: // Risod, Akola
                        case 7252: // Washim, Akola
                        case 7253: // Mangrulpur, Akola
                        case 7254: // Malgaon, Akola
                        case 7255: // Barshi Takli, Akola
                        case 7256: // Murtizapur, Akola
                        case 7257: // Balapur, Akola
                        case 7258: // Akot, Akola
                        case 7260: // Lonar, Buldhana
                        case 7261: // Deolgaonraja, Buldhana
                        case 7262: // Buldhana, Buldhana
                        case 7263: // Khamgaon, Buldhana
                        case 7264: // Chikhali, Buldhana
                        case 7266: // Jalgaonjamod, Buldhana
                        case 7267: // Malkapur, Buldhana
                        case 7268: // Mekhar, Buldhana
                        case 7269: // Sindkhedaraja, Buldhana
                        case 7270: // Sonkatch, Dewas
                        case 7271: // Bagli, Dewas
                        case 7272: // Dewas, Dewas
                        case 7273: // Kannod, Dewas
                        case 7274: // Khategaon, Dewas
                        case 7279: // Nandnva, Buldhana
                        case 7280: // Barwaha, Khargone
                        case 7281: // Sendhwa, Khargone
                        case 7282: // Khargone, Khargone
                        case 7283: // Maheshwar, Khargone
                        case 7284: // Rajpur, Khargone
                        case 7285: // Kasrawad, Khargone
                        case 7286: // Khetia, Khargone
                        case 7287: // Gogaon, Khargone
                        case 7288: // Bhikangaon, Khargone
                        case 7289: // Zhirnia, Khargone
                        case 7290: // Badwani, Khargone
                        case 7291: // Manawar, Dhar
                        case 7292: // Dhar, Dhar
                        case 7294: // Dharampuri, Dhar
                        case 7295: // Badnawar, Dhar
                        case 7296: // Sardarpur, Dhar
                        case 7297: // Kukshi, Dhar
                        case 7320: // Pandhana, Khandwa
                        case 7321: // Sanwer, Indore
                        case 7322: // Depalpur, Indore
                        case 7323: // Punasa, Khandwa
                        case 7324: // Mhow, Indore
                        case 7325: // Burhanpur, Khandwa
                        case 7326: // Baldi, Khandwa
                        case 7327: // Harsud, Khandwa
                        case 7328: // Khalwa, Khandwa
                        case 7329: // Khakner, Khandwa
                        case 7360: // Shujalpur, Shajapur
                        case 7361: // Susner, Shajapur
                        case 7362: // Agar, Shajapur
                        case 7363: // Berchha, Shajapur
                        case 7364: // Shajapur, Shajapur
                        case 7365: // Mahidpurcity, Ujjain
                        case 7366: // Khachrod, Ujjain
                        case 7367: // Badnagar, Ujjain
                        case 7368: // Ghatia, Ujjain
                        case 7369: // Tarana, Ujjain
                        case 7370: // Khilchipur, Rajgarh
                        case 7371: // Sarangpur, Rajgarh
                        case 7372: // Rajgarh, Rajgarh
                        case 7374: // Biaora, Rajgarh
                        case 7375: // Narsingharh, Rajgarh
                        case 7390: // Thandla, Jhabua
                        case 7391: // Petlawad, Jhabua
                        case 7392: // Jhabua, Jhabua
                        case 7393: // Jobat, Jhabua
                        case 7394: // Alirajpur, Jhabua
                        case 7395: // Sondhwa, Jhabua
                        case 7410: // Alot, Ratlam
                        case 7412: // Ratlam, Ratlam
                        case 7413: // Sailana, Ratlam
                        case 7414: // Jaora, Ratlam
                        case 7420: // Jawad, Mandsaur
                        case 7421: // Manasa, Mandsaur
                        case 7422: // Mandsaur, Mandsaur
                        case 7423: // Neemuch, Mandsaur
                        case 7424: // Malhargarh, Mandsaur
                        case 7425: // Garoth, Mandsaur
                        case 7426: // Sitamau, Mandsaur
                        case 7427: // Bhanpura, Mandsaur
                        case 7430: // Khanpur, Jhalawar
                        case 7431: // Aklera, Jhalawar
                        case 7432: // Jhalawar, Jhalawar
                        case 7433: // Pachpahar (Bhawanimandi), Jhalawar
                        case 7434: // Pirawa (Raipur), Jhalawar
                        case 7435: // Gangdhar, Jhalawar
                        case 7436: // Hindoli, Bundi
                        case 7437: // Nainwa, Bundi
                        case 7438: // Keshoraipatan (Patan), Bundi
                        case 7450: // Sangod, Kota
                        case 7451: // Atru, Kota
                        case 7452: // Chhabra, Kota
                        case 7453: // Baran, Kota
                        case 7454: // Chhipaborad, Kota
                        case 7455: // Digod (Sultanpur), Kota
                        case 7456: // Kishanganj (Bhanwargarh), Kota
                        case 7457: // Mangrol, Kota
                        case 7458: // Pipalda (Sumerganj Mandi), Kota
                        case 7459: // Ramganj Mandi, Kota
                        case 7460: // Sahabad, Kota
                        case 7461: // Mahuwa, Sawaimadhopur
                        case 7462: // Sawaimadhopur, Sawaimadhopur
                        case 7463: // Gangapur, Sawaimadhopur
                        case 7464: // Karauli, Sawaimadhopur
                        case 7465: // Sapotra, Sawaimadhopur
                        case 7466: // Bonli, Sawaimadhopur
                        case 7467: // Bamanwas, Sawaimadhopur
                        case 7468: // Khandar, Sawaimadhopur
                        case 7469: // Hindaun, Sawaimadhopur
                        case 7480: // Goharganj, Raisen
                        case 7481: // Gairatganj, Raisen
                        case 7482: // Raisen, Raisen
                        case 7484: // Silwani, Raisen
                        case 7485: // Udaipura, Raisen
                        case 7486: // Bareli, Raisen
                        case 7487: // Begamganj, Raisen
                        case 7490: // Pohari, Shivpuri
                        case 7491: // Narwar, Shivpuri
                        case 7492: // Shivpuri, Shivpuri
                        case 7493: // Karera, Shivpuri
                        case 7494: // Kolaras, Shivpuri
                        case 7495: // Badarwas, Shivpuri
                        case 7496: // Pichhore, Shivpuri
                        case 7497: // Khaniadhana, Shivpuri
                        case 7521: // Seondha, Gwalior
                        case 7522: // Datia, Gwalior
                        case 7523: // Bhander, Gwalior
                        case 7524: // Dabra, Gwalior
                        case 7525: // Bhitarwar, Gwalior
                        case 7526: // Ghatigaon, Gwalior
                        case 7527: // Mehgaon, Morena
                        case 7528: // Bijaypur, Morena
                        case 7529: // Laher, Morena
                        case 7530: // Sheopurkalan, Morena
                        case 7531: // Baroda, Morena
                        case 7532: // Morena, Morena
                        case 7533: // Karhal, Morena
                        case 7534: // Bhind, Morena
                        case 7535: // Raghunathpur, Morena
                        case 7536: // Sabalgarh, Morena
                        case 7537: // Jora, Morena
                        case 7538: // Ambah, Morena
                        case 7539: // Gohad, Morena
                        case 7540: // Bamori, Guna
                        case 7541: // Isagarh, Guna
                        case 7542: // Guna, Guna
                        case 7543: // Ashoknagar, Guna
                        case 7544: // Raghogarh, Guna
                        case 7545: // Arone, Guna
                        case 7546: // Chachaura, Guna
                        case 7547: // Chanderi, Guna
                        case 7548: // Mungaoli, Guna
                        case 7560: // Ashta, Bhopal
                        case 7561: // Ichhawar, Bhopal
                        case 7562: // Sehore, Bhopal
                        case 7563: // Nasrullaganj, Bhopal
                        case 7564: // Budhni, Bhopal
                        case 7565: // Berasia, Bhopal
                        case 7570: // Seonimalwa, Itarsi
                        case 7571: // Khirkiya, Itarsi
                        case 7572: // Itarsi, Itarsi
                        case 7573: // Timarani, Itarsi
                        case 7574: // Hoshangabad, Itarsi
                        case 7575: // Sohagpur, Itarsi
                        case 7576: // Piparia, Itarsi
                        case 7577: // Harda, Itarsi
                        case 7578: // Pachmarhi, Itarsi
                        case 7580: // Bina, Sagar
                        case 7581: // Khurai, Sagar
                        case 7582: // Sagar, Sagar
                        case 7583: // Banda, Sagar
                        case 7584: // Rahatgarh, Sagar
                        case 7585: // Rehli, Sagar
                        case 7586: // Deori, Sagar
                        case 7590: // Lateri, Vidisha
                        case 7591: // Sironj, Vidisha
                        case 7592: // Vidisha, Vidisha
                        case 7593: // Kurwai, Vidisha
                        case 7594: // Ganjbasoda, Vidisha
                        case 7595: // Nateran, Vidisha
                        case 7596: // Gyraspur, Vidisha
                        case 7601: // Patharia, Damoh
                        case 7603: // Tendukheda, Damoh
                        case 7604: // Hatta, Damoh
                        case 7605: // Patera, Damoh
                        case 7606: // Jabera, Damoh
                        case 7608: // Bijawar, Chhatarpur
                        case 7609: // Buxwaha, Chhatarpur
                        case 7621: // Patan, Jabalpur
                        case 7622: // Katni, Jabalpur
                        case 7623: // Kundam, Jabalpur
                        case 7624: // Sihora, Jabalpur
                        case 7625: // Umariapan, Jabalpur
                        case 7626: // Vijayraghogarh, Jabalpur
                        case 7627: // Manpur, Shahdol
                        case 7628: // Karpa, Shahdol
                        case 7629: // Pushprajgarh, Shahdol
                        case 7630: // Katangi, Balaghat
                        case 7632: // Balaghat, Balaghat
                        case 7633: // Waraseoni, Balaghat
                        case 7634: // Lamta, Balaghat
                        case 7635: // Lanji, Balaghat
                        case 7636: // Baihar, Balaghat
                        case 7637: // Birsa, Balaghat
                        case 7638: // Damoh, Balaghat
                        case 7640: // Shahpur, Mandla
                        case 7641: // Niwas, Mandla
                        case 7642: // Mandla, Mandla
                        case 7643: // Bijadandi, Mandla
                        case 7644: // Dindori, Mandla
                        case 7645: // Karanjia, Mandla
                        case 7646: // Nainpur, Mandla
                        case 7647: // Ghughari, Mandla
                        case 7648: // Mawai, Mandla
                        case 7649: // Kakaiya, Mandla
                        case 7650: // Beohari, Shahdol
                        case 7651: // Jaisinghnagar, Shahdol
                        case 7652: // Shahdol, Shahdol
                        case 7653: // Bandhavgarh, Shahdol
                        case 7655: // Birsinghpur, Shahdol
                        case 7656: // Kannodi, Shahdol
                        case 7657: // Jaitpur, Shahdol
                        case 7658: // Kotma, Shahdol
                        case 7659: // Jaithari, Shahdol
                        case 7660: // Sirmour, Rewa
                        case 7661: // Teonthar, Rewa
                        case 7662: // Rewa, Rewa
                        case 7663: // Mauganj, Rewa
                        case 7664: // Hanumana, Rewa
                        case 7670: // Majhagwan, Satna
                        case 7671: // Jaitwara, Satna
                        case 7672: // Satna, Satna
                        case 7673: // Nagod, Satna
                        case 7674: // Maihar, Satna
                        case 7675: // Amarpatan, Satna
                        case 7680: // Niwari, Chhatarpur
                        case 7681: // Jatara, Chhatarpur
                        case 7682: // Chhatarpur, Chhatarpur
                        case 7683: // Tikamgarh, Chhatarpur
                        case 7684: // Baldeogarh, Chhatarpur
                        case 7685: // Nowgaon, Chhatarpur
                        case 7686: // Khajuraho, Chhatarpur
                        case 7687: // Laundi, Chhatarpur
                        case 7688: // Gourihar, Chhatarpur
                        case 7689: // Badamalhera, Chhatarpur
                        case 7690: // Lakhnadon, Seoni
                        case 7691: // Chhapara, Seoni
                        case 7692: // Seoni, Seoni
                        case 7693: // Ghansour, Seoni
                        case 7694: // Keolari, Seoni
                        case 7695: // Gopalganj, Seoni
                        case 7700: // Nagri, Raipur
                        case 7701: // Pingeshwar, Raipur
                        case 7703: // Manpur, Raipur
                        case 7704: // Deobhog, Raipur
                        case 7705: // Kurud, Raipur
                        case 7706: // Gariaband, Raipur
                        case 7707: // Bagbahera, Raipur
                        case 7720: // Arang, Raipur
                        case 7721: // Neora, Raipur
                        case 7722: // Dhamtari, Raipur
                        case 7723: // Mahasamund, Raipur
                        case 7724: // Basana, Raipur
                        case 7725: // Saraipali, Raipur
                        case 7726: // Bhatapara, Raipur
                        case 7727: // Balodabazar, Raipur
                        case 7728: // Kasdol, Raipur
                        case 7729: // Bhilaigarh, Raipur
                        case 7730: // Ajaigarh, Panna
                        case 7731: // Gunnore, Panna
                        case 7732: // Panna, Panna
                        case 7733: // Pawai, Panna
                        case 7734: // Shahnagar, Panna
                        case 7740: // Bodla, Durg
                        case 7741: // Kawardha, Durg
                        case 7743: // Chuikhadan, Durg
                        case 7744: // Rajandgaon, Durg
                        case 7745: // Chhuriakala, Durg
                        case 7746: // Manpur, Durg
                        case 7747: // Mohla, Durg
                        case 7748: // Dallirajhara, Durg
                        case 7749: // Balod, Durg
                        case 7750: // Marwahi, Bilaspur
                        case 7751: // Pendra, Bilaspur
                        case 7752: // Bilaspur, Bilaspur
                        case 7753: // Kota, Bilaspur
                        case 7754: // Pandaria, Bilaspur
                        case 7755: // Mungeli, Bilaspur
                        case 7756: // Lormi, Bilaspur
                        case 7757: // Shakti, Bilaspur
                        case 7758: // Dabhara, Bilaspur
                        case 7759: // Korba, Bilaspur
                        case 7761: // Tapkara, Raigarh
                        case 7762: // Raigarh, Raigarh
                        case 7763: // Jashpurnagar, Raigarh
                        case 7764: // Kunkuri, Raigarh
                        case 7765: // Pathalgaon, Raigarh
                        case 7766: // Dharamjaigarh, Raigarh
                        case 7767: // Gharghoda, Raigarh
                        case 7768: // Saranggarh, Raigarh
                        case 7769: // Bagicha, Raigarh
                        case 7770: // Kathdol, Sarguja (Ambikapur)
                        case 7771: // Manendragarh, Sarguja (Ambikapur)
                        case 7772: // Wadrainagar, Sarguja (Ambikapur)
                        case 7773: // Odgi, Sarguja (Ambikapur)
                        case 7774: // Ambikapur, Sarguja (Ambikapur)
                        case 7775: // Surajpur, Sarguja (Ambikapur)
                        case 7776: // Premnagar, Sarguja (Ambikapur)
                        case 7777: // Pratappur, Sarguja (Ambikapur)
                        case 7778: // Semaria, Sarguja (Ambikapur)
                        case 7779: // Ramchandrapur, Sarguja (Ambikapur)
                        case 7780: // Pakhanjur, Jagdalpur
                        case 7781: // Narainpur, Jagdalpur
                        case 7782: // Jagdalpur, Jagdalpur
                        case 7783: // Padamkot, Jagdalpur
                        case 7784: // Parasgaon, Jagdalpur
                        case 7785: // Makodi, Jagdalpur
                        case 7786: // Kondagaon, Jagdalpur
                        case 7787: // Jarwa, Jagdalpur
                        case 7788: // Luckwada, Jagdalpur
                        case 7789: // Bhairongarh, Jagdalpur
                        case 7790: // Babaichichli, Narsinghpur
                        case 7791: // Gadarwara, Narsinghpur
                        case 7792: // Narsinghpur, Narsinghpur
                        case 7793: // Kareli, Narsinghpur
                        case 7794: // Gotegaon, Narsinghpur
                        case 7801: // Deosar, Sidhi
                        case 7802: // Churhat, Sidhi
                        case 7803: // Majholi, Sidhi
                        case 7804: // Kusmi, Sidhi
                        case 7805: // Singrauli, Sidhi
                        case 7806: // Chitrangi, Sidhi
                        case 7810: // Uproda, Bilaspur
                        case 7811: // Pasan, Bilaspur
                        case 7812: // Damoh, Damoh
                        case 7813: // Barpalli, Bilaspur
                        case 7815: // Kathghora, Bilaspur
                        case 7816: // Pali, Bilaspur
                        case 7817: // Janjgir, Bilaspur
                        case 7818: // Chandipara, Bilaspur
                        case 7819: // Pandishankar, Bilaspur
                        case 7820: // Khairagarh, Durg
                        case 7821: // Dhamda, Durg
                        case 7822: // Sidhi, Sidhi
                        case 7823: // Dongargarh, Durg
                        case 7824: // Bemetara, Durg
                        case 7825: // Berla, Durg
                        case 7826: // Patan, Durg
                        case 7831: // Balrampur, Sarguja (Ambikapur)
                        case 7832: // Rajpur, Sarguja (Ambikapur)
                        case 7833: // Udaipur, Sarguja (Ambikapur)
                        case 7834: // Sitapur, Sarguja (Ambikapur)
                        case 7835: // Bharathpur, Sarguja (Ambikapur)
                        case 7836: // Baikunthpur, Sarguja (Ambikapur)
                        case 7840: // Koyelibeda, Jagdalpur
                        case 7841: // Sarona, Jagdalpur
                        case 7843: // Durgakondal, Jagdalpur
                        case 7844: // Pakhanjur, Jagdalpur
                        case 7846: // Garpa, Jagdalpur
                        case 7847: // Antagarh, Jagdalpur
                        case 7848: // Keskal, Jagdalpur
                        case 7849: // Baderajpur, Jagdalpur
                        case 7850: // Bhanupratappur, Jagdalpur
                        case 7851: // Bhopalpatnam, Jagdalpur
                        case 7852: // Toynar, Jagdalpur
                        case 7853: // Bijapur, Jagdalpur
                        case 7854: // Ilamidi, Jagdalpur
                        case 7855: // Chingmut, Jagdalpur
                        case 7856: // Dantewada, Jagdalpur
                        case 7857: // Bacheli, Jagdalpur
                        case 7858: // Kuakunda, Jagdalpur
                        case 7859: // Lohadigundah, Jagdalpur
                        case 7861: // Netanar, Jagdalpur
                        case 7862: // Bastanar, Jagdalpur
                        case 7863: // Chingamut, Jagdalpur
                        case 7864: // Sukma, Jagdalpur
                        case 7865: // Gogunda, Jagdalpur
                        case 7866: // Konta, Jagdalpur
                        case 7867: // Bokaband, Jagdalpur
                        case 7868: // Kanker, Jagdalpur
                        case 8110: // Anekal, Bangalore
                        case 8111: // Hosakote, Bangalore
                        case 8113: // Channapatna, Bangalore
                        case 8117: // Kanakapura, Bangalore
                        case 8118: // Nelamangala, Bangalore
                        case 8119: // Doddaballapur, Bangalore
                        case 8131: // Gubbi, Tumkur
                        case 8132: // Kunigal, Tumkur
                        case 8133: // Chikkanayakanahalli, Tumkur
                        case 8134: // Tiptur, Tumkur
                        case 8135: // Sira, Tumkur
                        case 8136: // Pavagada, Tumkur
                        case 8137: // Madugiri, Tumkur
                        case 8138: // Koratageri, Tumkur
                        case 8139: // Turuvekere, Tumkur
                        case 8150: // Bagepalli, Kolar
                        case 8151: // Malur, Kolar
                        case 8152: // Kolar, Kolar
                        case 8153: // Bangarpet, Kolar
                        case 8154: // Chintamani, Kolar
                        case 8155: // Gowribidanur, Kolar
                        case 8156: // Chikkaballapur, Kolar
                        case 8157: // Srinivasapur, Kolar
                        case 8158: // Sidlaghatta, Kolar
                        case 8159: // Mulbagal, Kolar
                        case 8170: // Alur, Hassan
                        case 8172: // Hassan, Hassan
                        case 8173: // Sakleshpur, Hassan
                        case 8174: // Arsikere, Hassan
                        case 8175: // Holenarasipur, Hassan
                        case 8176: // Cannarayapatna, Hassan
                        case 8177: // Belur, Hassan
                        case 8180: // Basavapatna, Shimoga
                        case 8181: // Thirthahalli, Shimoga
                        case 8182: // Shimoga, Shimoga
                        case 8183: // Sagar, Shimoga
                        case 8184: // Sorab, Shimoga
                        case 8185: // Hosanagara, Shimoga
                        case 8186: // Kargal, Shimoga
                        case 8187: // Shikaripura, Shimoga
                        case 8188: // Honnali, Shimoga
                        case 8189: // Channagiri, Shimoga
                        case 8190: // Tallak, Devangere
                        case 8191: // Holalkere, Devangere
                        case 8192: // Davangere, Devangere
                        case 8193: // Hiriyur, Devangere
                        case 8194: // Chitradurga, Devangere
                        case 8195: // Challakere, Devangere
                        case 8196: // Jagalur, Devangere
                        case 8198: // Molkalmuru, Devangere
                        case 8199: // Hosadurga, Devangere
                        case 8221: // Nanjangud, Mysore
                        case 8222: // Hunsur, Mysore
                        case 8223: // K.R.Nagar, Mysore
                        case 8224: // Kollegal, Mysore
                        case 8225: // Cowdahalli, Mysore
                        case 8226: // Chamrajnagar, Mysore
                        case 8227: // T.Narsipur, Mysore
                        case 8228: // H.D.Kote, Mysore
                        case 8229: // Gundlupet, Mysore
                        case 8230: // Krishnarajapet, Mandya
                        case 8231: // Malavalli, Mandya
                        case 8232: // Mandya, Mandya
                        case 8234: // Nagamangala, Mandya
                        case 8236: // Pandavpura, Mandya
                        case 8251: // Puttur, Dakshin Kanada (Mangalore)
                        case 8253: // Hebri, Dakshin Kanada (Mangalore)
                        case 8254: // Kundapur, Dakshin Kanada (Mangalore)
                        case 8255: // Bantwal, Dakshin Kanada (Mangalore)
                        case 8256: // Belthangady, Dakshin Kanada (Mangalore)
                        case 8257: // Sullia, Dakshin Kanada (Mangalore)
                        case 8258: // Karkala, Dakshin Kanada (Mangalore)
                        case 8259: // Shankarnarayana, Dakshin Kanada (Mangalore)
                        case 8261: // Tarikere, Chikmagalur
                        case 8262: // Chikmagalur, Chikmagalur
                        case 8263: // Mudigere, Chikmagalur
                        case 8265: // Koppa, Chikmagalur
                        case 8266: // Narsimharajapur, Chikmagalur
                        case 8267: // Kadur, Chikmagalur
                        case 8272: // Madikeri, Kodagu (Madikera)
                        case 8274: // Virajpet, Kodagu (Madikera)
                        case 8276: // Somwarpet, Kodagu (Madikera)
                        case 8282: // Bhadravati, Shimoga
                        case 8283: // Salkani, Uttar Kanada (Karwar)
                        case 8284: // Haliyal, Uttar Kanada (Karwar)
                        case 8288: // Bailhongal, Belgaum
                        case 8289: // Athani, Belgaum
                        case 8301: // Mundagod, Uttar Kanada (Karwar)
                        case 8304: // Kundgol, Hubli
                        case 8330: // Saundatti, Belgaum
                        case 8331: // Raibag (Kudchi), Belgaum
                        case 8332: // Gokak, Belgaum
                        case 8333: // Hukkeri (Sankeshwar), Belgaum
                        case 8334: // Mudalgi, Belgaum
                        case 8335: // Ramdurg, Belgaum
                        case 8336: // Khanapur, Belgaum
                        case 8337: // Murugod, Belgaum
                        case 8338: // Chikkodi, Belgaum
                        case 8339: // Ainapur, Belgaum
                        case 8342: // Margao, Panji
                        case 8343: // Ponda, Panji
                        case 8345: // Sanguem, Panji
                        case 8346: // Canacona (Quepem), Panji
                        case 8350: // Mudhol, Bijapur
                        case 8351: // Hungund, Bijapur
                        case 8352: // Bijapur, Bijapur
                        case 8353: // Jamkhandi, Bijapur
                        case 8354: // Bagalkot, Bijapur
                        case 8355: // Bableshwar, Bijapur
                        case 8356: // Muddebihal, Bijapur
                        case 8357: // Badami, Bijapur
                        case 8358: // Basavanabagewadi, Bijapur
                        case 8359: // Indi, Bijapur
                        case 8370: // Kalghatagi, Hubli
                        case 8371: // Mundargi, Hubli
                        case 8372: // Gadag, Hubli
                        case 8373: // Ranebennur, Hubli
                        case 8375: // Haveri, Hubli
                        case 8376: // Hirekerur, Hubli
                        case 8377: // Nargund, Hubli
                        case 8378: // Savanur, Hubli
                        case 8379: // Hangal, Hubli
                        case 8380: // Navalgund, Hubli
                        case 8381: // Ron, Hubli
                        case 8382: // Karwar, Uttar Kanada (Karwar)
                        case 8383: // Joida, Uttar Kanada (Karwar)
                        case 8384: // Sirsi, Uttar Kanada (Karwar)
                        case 8385: // Bhatkal, Uttar Kanada (Karwar)
                        case 8386: // Kumta, Uttar Kanada (Karwar)
                        case 8387: // Honnavar, Uttar Kanada (Karwar)
                        case 8388: // Ankola, Uttar Kanada (Karwar)
                        case 8389: // Siddapur, Uttar Kanada (Karwar)
                        case 8391: // Kudligi, Bellary
                        case 8392: // Bellary, Bellary
                        case 8393: // Kurugodu, Bellary
                        case 8394: // Hospet, Bellary
                        case 8395: // Sandur, Bellary
                        case 8396: // Siruguppa, Bellary
                        case 8397: // H.B.Halli, Bellary
                        case 8398: // Harapanahalli, Bellary
                        case 8399: // Huvinahadagali, Bellary
                        case 8402: // Kanigiri, Ongole
                        case 8403: // Yerragondapalem, Ongole
                        case 8404: // Marturu, Ongole
                        case 8405: // Giddalur, Ongole
                        case 8406: // Cumbum, Ongole
                        case 8407: // Darsi, Ongole
                        case 8408: // Donakonda, Ongole
                        case 8411: // Tanduru, Hyderabad
                        case 8412: // Pargi, Hyderabad
                        case 8413: // Hyderabadwest (Shamshabad), Hyderabad
                        case 8414: // Ibrahimpatnam, Hyderabad
                        case 8415: // Hyderabadeast (Ghatkeswar), Hyderabad
                        case 8416: // Vikrabad, Hyderabad
                        case 8417: // Chevella, Hyderabad
                        case 8418: // Medchal, Hyderabad
                        case 8419: // Yellapur, Uttar Kanada (Karwar)
                        case 8422: // Chadchan, Bijapur
                        case 8424: // Devarahippargi, Bijapur
                        case 8425: // Biligi, Bijapur
                        case 8426: // Telgi, Bijapur
                        case 8440: // Nimburga, Gulbarga
                        case 8441: // Sedam, Gulbarga
                        case 8442: // Jewargi, Gulbarga
                        case 8443: // Shorapur, Gulbarga
                        case 8444: // Hunsagi, Gulbarga
                        case 8450: // Andole (Jogipet), Sangareddy
                        case 8451: // Zahirabad, Sangareddy
                        case 8452: // Medak, Sangareddy
                        case 8454: // Gajwel, Sangareddy
                        case 8455: // Sangareddy, Sangareddy
                        case 8456: // Narayankhed, Sangareddy
                        case 8457: // Siddipet, Sangareddy
                        case 8458: // Narsapur, Sangareddy
                        case 8461: // Dichpalli, Nizamabad
                        case 8462: // Nizamabad, Nizamabad
                        case 8463: // Armoor, Nizamabad
                        case 8464: // Madnur, Nizamabad
                        case 8465: // Yellareddy, Nizamabad
                        case 8466: // Banswada, Nizamabad
                        case 8467: // Bodhan, Nizamabad
                        case 8468: // Kamareddy, Nizamabad
                        case 8470: // Afzalpur, Gulbarga
                        case 8471: // Mashal, Gulbarga
                        case 8472: // Gulbarga, Gulbarga
                        case 8473: // Yadgiri, Gulbarga
                        case 8474: // Chittapur, Gulbarga
                        case 8475: // Chincholi, Gulbarga
                        case 8476: // Wadi, Gulbarga
                        case 8477: // Aland, Gulbarga
                        case 8478: // Kamalapur, Gulbarga
                        case 8479: // Shahapur, Gulbarga
                        case 8481: // Basavakalyan, Bidar
                        case 8482: // Bidar, Bidar
                        case 8483: // Humnabad, Bidar
                        case 8484: // Bhalki, Bidar
                        case 8485: // Aurad, Bidar
                        case 8487: // Shirahatti, Hubli
                        case 8488: // Sindagi, Bijapur
                        case 8490: // Pamuru, Ongole
                        case 8491: // Kanaganapalle, Anantpur (Guntakal)
                        case 8492: // Kambadur, Anantpur (Guntakal)
                        case 8493: // Madakasira, Anantpur (Guntakal)
                        case 8494: // Kadiri, Anantpur (Guntakal)
                        case 8495: // Rayadurg, Anantpur (Guntakal)
                        case 8496: // Uravakonda, Anantpur (Guntakal)
                        case 8497: // Kalyandurg, Anantpur (Guntakal)
                        case 8498: // Nallacheruvu (Tanakallu), Anantpur (Guntakal)
                        case 8499: // Podili, Ongole
                        case 8501: // Kollapur, Mahabubnagar
                        case 8502: // Alampur, Mahabubnagar
                        case 8503: // Makthal, Mahabubnagar
                        case 8504: // Atmakur, Mahabubnagar
                        case 8505: // Kodangal, Mahabubnagar
                        case 8506: // Narayanpet, Mahabubnagar
                        case 8510: // Koilkuntla, Kurnool
                        case 8512: // Adoni, Kurnool
                        case 8513: // Nandikotkur, Kurnool
                        case 8514: // Nandyal, Kurnool
                        case 8515: // Banaganapalle, Kurnool
                        case 8516: // Dronachalam, Kurnool
                        case 8517: // Atmakur, Kurnool
                        case 8518: // Kurnool, Kurnool
                        case 8519: // Allagadda, Kurnool
                        case 8520: // Pattikonda, Kurnool
                        case 8522: // Peapalle, Kurnool
                        case 8523: // Alur, Kurnool
                        case 8524: // Srisailam, Kurnool
                        case 8525: // Gudur (Kodumur), Kurnool
                        case 8531: // Deodurga, Raichur
                        case 8532: // Raichur, Raichur
                        case 8533: // Gangavathi, Raichur
                        case 8534: // Yelburga, Raichur
                        case 8535: // Sindhanur, Raichur
                        case 8536: // Kustagi, Raichur
                        case 8537: // Lingsugur, Raichur
                        case 8538: // Manvi, Raichur
                        case 8539: // Koppal, Raichur
                        case 8540: // Nagarkurnool, Mahabubnagar
                        case 8541: // Achampet, Mahabubnagar
                        case 8542: // Mahabubnagar, Mahabubnagar
                        case 8543: // Wanaparthy, Mahabubnagar
                        case 8545: // Amangallu, Mahabubnagar
                        case 8546: // Gadwal, Mahabubnagar
                        case 8548: // Shadnagar, Mahabubnagar
                        case 8549: // Kalwakurthy, Mahabubnagar
                        case 8550: // Yellanuru, Anantpur (Guntakal)
                        case 8551: // Garladinne, Anantpur (Guntakal)
                        case 8552: // Gooty (Guntakal), Anantpur (Guntakal)
                        case 8554: // Anantapur, Anantpur (Guntakal)
                        case 8556: // Hindupur, Anantpur (Guntakal)
                        case 8557: // Penukonda, Anantpur (Guntakal)
                        case 8558: // Tadipatri, Anantpur (Guntakal)
                        case 8559: // Dharmavaram, Anantpur (Guntakal)
                        case 8560: // Jammalamadugu, Cuddapah
                        case 8561: // Rayachoti, Cuddapah
                        case 8562: // Cuddapah, Cuddapah
                        case 8563: // Kamalapuram (Yerraguntala), Cuddapah
                        case 8564: // Proddatur, Cuddapah
                        case 8565: // Rajampeta, Cuddapah
                        case 8566: // Koduru, Cuddapah
                        case 8567: // Lakkireddipalli, Cuddapah
                        case 8568: // Pulivendla, Cuddapah
                        case 8569: // Badvel, Cuddapah
                        case 8570: // Kuppam, Chittoor
                        case 8571: // Madanapalli, Chittoor
                        case 8572: // Chittoor, Chittoor
                        case 8573: // Bangarupalem, Chittoor
                        case 8576: // Satyavedu, Chittoor
                        case 8577: // Putturu, Chittoor
                        case 8578: // Srikalahasthi, Chittoor
                        case 8579: // Palmaneru, Chittoor
                        case 8581: // Punganur, Chittoor
                        case 8582: // B.Kothakota, Chittoor
                        case 8583: // Sodam, Chittoor
                        case 8584: // Piler, Chittoor
                        case 8585: // Pakala, Chittoor
                        case 8586: // Vayalpad, Chittoor
                        case 8587: // Venkatgirikota, Chittoor
                        case 8588: // Vaimpalli, Cuddapah
                        case 8589: // Siddavattam, Cuddapah
                        case 8592: // Ongole, Ongole
                        case 8593: // Medarmetla, Ongole
                        case 8594: // Chirala, Ongole
                        case 8596: // Markapur, Ongole
                        case 8598: // Kandukuru, Ongole
                        case 8599: // Ulvapadu, Ongole
                        case 8620: // Udaygiri, Nellore
                        case 8621: // Rapur (Podalakur), Nellore
                        case 8622: // Kovvur, Nellore
                        case 8623: // Sullurpet, Nellore
                        case 8624: // Gudur, Nellore
                        case 8625: // Venkatgiri, Nellore
                        case 8626: // Kavali, Nellore
                        case 8627: // Atmakur, Nellore
                        case 8628: // Chejerla, Nellore
                        case 8629: // Vinjamuru, Nellore
                        case 8640: // Krosuru, Guntur
                        case 8641: // Sattenapalli, Guntur
                        case 8642: // Palnad (Macherala), Guntur
                        case 8643: // Bapatla, Guntur
                        case 8644: // Tenali, Guntur
                        case 8645: // Mangalagiri, Guntur
                        case 8646: // Vinukonda, Guntur
                        case 8647: // Narsaraopet, Guntur
                        case 8648: // Repalle, Guntur
                        case 8649: // Piduguralla, Guntur
                        case 8654: // Jaggayyapet, Vijayawada
                        case 8656: // Nuzvidu, Vijayawada
                        case 8659: // Mylavaram, Vijayawada
                        case 8671: // Divi (Challapalli), Vijayawada
                        case 8672: // Bandar (Machilipatnam), Vijayawada
                        case 8673: // Tirivuru, Vijayawada
                        case 8674: // Gudivada, Vijayawada
                        case 8676: // Vuyyuru, Vijayawada
                        case 8677: // Kaikaluru, Vijayawada
                        case 8678: // Nandigama, Vijayawada
                        case 8680: // Nidamanur (Hillcolony), Nalgonda
                        case 8681: // Chandoor, Nalgonda
                        case 8682: // Nalgonda, Nalgonda
                        case 8683: // Hazurnagar, Nalgonda
                        case 8684: // Suryapet, Nalgonda
                        case 8685: // Bhongir, Nalgonda
                        case 8689: // Miryalguda, Nalgonda
                        case 8691: // Devarakonda, Nalgonda
                        case 8692: // Nampalle, Nalgonda
                        case 8693: // Thungaturthy, Nalgonda
                        case 8694: // Ramannapet, Nalgonda
                        case 8710: // Cherial, Warangal
                        case 8711: // Wardhannapet (Ghanapur), Warangal
                        case 8713: // Parkal, Warangal
                        case 8715: // Mulug, Warangal
                        case 8716: // Jangaon, Warangal
                        case 8717: // Eturnagaram, Warangal
                        case 8718: // Narasampet, Warangal
                        case 8719: // Mahabubbad, Warangal
                        case 8720: // Mahadevapur, Karimnagar
                        case 8721: // Husnabad, Karimnagar
                        case 8723: // Sircilla, Karimnagar
                        case 8724: // Jagtial, Karimnagar
                        case 8725: // Metpalli, Karimnagar
                        case 8727: // Huzurabad, Karimnagar
                        case 8728: // Peddapalli, Karimnagar
                        case 8729: // Manthani, Karimnagar
                        case 8730: // Khanapur (Ap), Adilabad
                        case 8731: // Utnor, Adilabad
                        case 8732: // Adilabad, Adilabad
                        case 8733: // Asifabad, Adilabad
                        case 8734: // Nirmal, Adilabad
                        case 8735: // Bellampalli, Adilabad
                        case 8736: // Mancherial, Adilabad
                        case 8737: // Chinnor, Adilabad
                        case 8738: // Sirpurkagaznagar, Adilabad
                        case 8739: // Jannaram (Luxittipet), Adilabad
                        case 8740: // Aswaraopet, Khammam
                        case 8741: // Sudhimalla (Tekulapalli), Khammam
                        case 8742: // Khammam, Khammam
                        case 8743: // Bhadrachalam, Khammam
                        case 8744: // Kothagudem, Khammam
                        case 8745: // Yellandu, Khammam
                        case 8746: // Bhooragamphad (Manuguru), Khammam
                        case 8747: // Nuguru (Cherla), Khammam
                        case 8748: // .R.Puram, Khammam
                        case 8749: // Madhira, Khammam
                        case 8751: // Boath (Echoda), Adilabad
                        case 8752: // Bhainsa, Adilabad
                        case 8753: // Outsarangapalle, Adilabad
                        case 8761: // Sathupalli, Khammam
                        case 8811: // Polavaram, Eluru
                        case 8812: // Eluru, Eluru
                        case 8813: // Kovvur (Nidadavolu), Eluru
                        case 8814: // Narsapur (Palakole), Eluru
                        case 8816: // Bhimavaram, Eluru
                        case 8818: // Tadepalligudem, Eluru
                        case 8819: // Tanuku, Eluru
                        case 8821: // Jangareddygudem, Eluru
                        case 8823: // Chintalapudi, Eluru
                        case 8829: // Bhimadole, Eluru
                        case 8852: // Peddapuram, Rajahmundri
                        case 8854: // Tuni, Rajahmundri
                        case 8855: // Mandapeta (Ravulapalem), Rajahmundri
                        case 8856: // Amalapuram, Rajahmundri
                        case 8857: // Ramachandrapuram, Rajahmundri
                        case 8862: // Razole, Rajahmundri
                        case 8863: // Chavitidibbalu, Rajahmundri
                        case 8864: // Rampachodavaram, Rajahmundri
                        case 8865: // Yelavaram, Rajahmundri
                        case 8868: // Yeleswaram, Rajahmundri
                        case 8869: // Pithapuram, Rajahmundri
                        case 8922: // Vizayanagaram, Vizayanagaram
                        case 8924: // Anakapalle, Visakhapatnam
                        case 8931: // Yelamanchili, Visakhapatnam
                        case 8932: // Narsipatnam, Visakhapatnam
                        case 8933: // Bheemunipatnam, Visakhapatnam
                        case 8934: // Chodavaram, Visakhapatnam
                        case 8935: // Paderu, Visakhapatnam
                        case 8936: // Araku, Visakhapatnam
                        case 8937: // Chintapalle, Visakhapatnam
                        case 8938: // Sileru, Visakhapatnam
                        case 8941: // Palakonda (Rajam), Srikakulam
                        case 8942: // Srikakulam, Srikakulam
                        case 8944: // Bobbili, Vizayanagaram
                        case 8945: // Tekkali (Palasa), Srikakulam
                        case 8946: // Pathapatnam (Hiramandalam), Srikakulam
                        case 8947: // Sompeta, Srikakulam
                        case 8952: // Chepurupalli (Garividi), Vizayanagaram
                        case 8963: // Parvathipuram, Vizayanagaram
                        case 8964: // Saluru, Vizayanagaram
                        case 8965: // Gajapathinagaram, Vizayanagaram
                        case 8966: // Srungavarapukota (Kothvls), Vizayanagaram
                            parse_out->parse_state = STATE_INDIA_SUB_1_6;
                            parse_out->field_pos = 0;
                            parse_out->service_type = TSERV_CELL;
                        break;
                        default:
                            set_parse_error(parse_out, ERROR_AREA_CODE_NOT_FOUND, 103);
                        break;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_AREA_CODE_DIGITS_INVALID, 104);
                break;
            }
        break;
        case STATE_INDIA_SUB_1_4_4:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0)
                        parse_out->parse_state = STATE_INDIA_SUB_1_4_4;
                    else
                        set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 105);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_INDIA_SUB1);
                    if (parse_out->field_pos < 4) {
                        parse_out->parse_state = STATE_INDIA_SUB_1_4_4;
                    } else {
                        parse_out->parse_state = STATE_INDIA_SUB_2_4;
                        parse_out->field_pos = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 106);
                break;
            }
        break;
        case STATE_INDIA_SUB_1_3_4:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0)
                        parse_out->parse_state = STATE_INDIA_SUB_1_3_4;
                    else
                        set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 107);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_INDIA_SUB1);
                    if (parse_out->field_pos < 3) {
                        parse_out->parse_state = STATE_INDIA_SUB_1_3_4;
                    } else {
                        parse_out->parse_state = STATE_INDIA_SUB_2_4;
                        parse_out->field_pos = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 108);
                break;
            }
        break;
        case STATE_INDIA_SUB_1_2_4:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0)
                        parse_out->parse_state = STATE_INDIA_SUB_1_2_4;
                    else
                        set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 109);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_INDIA_SUB1);
                    if (parse_out->field_pos < 2) {
                        parse_out->parse_state = STATE_INDIA_SUB_1_2_4;
                    } else {
                        parse_out->parse_state = STATE_INDIA_SUB_2_4;
                        parse_out->field_pos = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 110);
                break;
            }
        break;
        case STATE_INDIA_SUB_2_4:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0)
                        parse_out->parse_state = STATE_INDIA_SUB_2_4;
                    else
                        set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_START2_INVALID, 111);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_INDIA_SUB2);
                    if (parse_out->field_pos < 4) {
                        parse_out->parse_state  = STATE_INDIA_SUB_2_4;
                    } else {
                        parse_out->parse_state  = STATE_EXTENSION_START;
                        parse_out->field_pos    = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 112);
                break;
            }
        break;
        case STATE_INDIA_SUB_1_6:
            switch (digit_value) {
                case DIGIT_SPECIAL:
                    if (parse_out->field_pos == 0)
                        parse_out->parse_state = STATE_INDIA_SUB_1_6;
                    else
                        set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_START_INVALID, 113);
                break;
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    set_parse_out(digit_letter, parse_out, FIELD_INDIA_SUB1);
                    if (parse_out->field_pos < 6) {
                        parse_out->parse_state  = STATE_INDIA_SUB_1_6;
                    } else {
                        parse_out->parse_state  = STATE_EXTENSION_START;
                        parse_out->field_pos    = 0;
                    }
                break;
                default:
                    set_parse_error(parse_out, ERROR_SUBSCRIBER_NUM_DIGITS_INVALID, 114);
                break;
            }
        break;
        default:
            set_parse_error(parse_out, ERROR_INVALID_STATE, 115);
        break;
    }
}

static char digit_value_to_text(char digit_value, char letter_index) {
    switch (digit_value) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
            if (letter_index == 0) {
                return digit_value + '0';
            } else {
                switch (digit_value) {
                    case 2:
                        switch (letter_index) {
                            case 1:
                                return 'A';
                            case 2:
                                return 'B';
                            case 3:
                                return 'C';
                            default:
                                return '?';
                        }
                    case 3:
                        switch (letter_index) {
                            case 1:
                                return 'D';
                            case 2:
                                return 'E';
                            case 3:
                                return 'F';
                            default:
                                return '?';
                        }
                    case 4:
                        switch (letter_index) {
                            case 1:
                                return 'G';
                            case 2:
                                return 'H';
                            case 3:
                                return 'I';
                            default:
                                return '?';
                        }
                    case 5:
                        switch (letter_index) {
                            case 1:
                                return 'J';
                            case 2:
                                return 'K';
                            case 3:
                                return 'L';
                            default:
                                return '?';
                        }
                    case 6:
                        switch (letter_index) {
                            case 1:
                                return 'M';
                            case 2:
                                return 'N';
                            case 3:
                                return 'O';
                            default:
                                return '?';
                        }
                    case 7:
                        switch (letter_index) {
                            case 1:
                                return 'P';
                            case 2:
                                return 'Q';
                            case 3:
                                return 'R';
                            case 4:
                                return 'S';
                            default:
                                return '?';
                        }
                    case 8:
                        switch (letter_index) {
                            case 1:
                                return 'T';
                            case 2:
                                return 'U';
                            case 3:
                                return 'V';
                            default:
                                return '?';
                        }
                    case 9:
                        switch (letter_index) {
                            case 1:
                                return 'W';
                            case 2:
                                return 'X';
                            case 3:
                                return 'Y';
                            case 4:
                                return 'Z';
                            default:
                                return '?';
                        }
                    default:
                        return '?';
                }
            }
        case DIGIT_STAR:
            return '*';
        case DIGIT_POUND:
            return '#';
        case DIGIT_PLUS:
            return '+';
        case DIGIT_PAUSE:
            return ',';
        case DIGIT_CONFIRM:
            return ';';
        case DIGIT_SPECIAL:
            return ' ';
        case DIGIT_SLASH:
            return '/';
        default:
            return '?';
    }
}

static int calling_code_get(struct parse_buffer * parse_in) {
    int calling_code;

    if (parse_in->digit_fields[0] != FIELD_CALLING_CODE)
        return 0;
    calling_code = parse_in->digit_values[0];

    if (parse_in->digit_fields[1] != FIELD_CALLING_CODE)
        return calling_code;
    calling_code = calling_code * 10 + parse_in->digit_values[1];

    if (parse_in->digit_fields[2] != FIELD_CALLING_CODE)
        return calling_code;
    calling_code = calling_code * 10 + parse_in->digit_values[2];

    if (parse_in->digit_fields[3] != FIELD_CALLING_CODE)
        return calling_code;
    calling_code = calling_code * 10 + parse_in->digit_values[3];

    if (parse_in->digit_fields[4] != FIELD_CALLING_CODE)
        return calling_code;
    calling_code = calling_code * 10 + parse_in->digit_values[4];

    return -1;
}

static uint additional_space_count(struct parse_buffer * parse_in) {
    uint additional_space_count = 0;
    int prior_field = FIELD_INVALID;
    int digit_index;
    for(digit_index = 0; digit_index < parse_in->digit_pos_next; ++digit_index) {
        uint value_mask         = parse_in->digit_valuesmask[digit_index];
        int  digit_field        = parse_in->digit_fields[digit_index];
        int  field_changed      = prior_field != digit_field;
        int  this_whitespace;

        if (field_changed)
            additional_space_count++;

        this_whitespace = (value_mask & VALUEMASK_WHITESPACE) == VALUEMASK_WHITESPACE;
        if (this_whitespace)
            additional_space_count++;

        this_whitespace = (value_mask & VALUEMASK_CONFIRM) == VALUEMASK_CONFIRM;
        if (this_whitespace)
            additional_space_count++;

        additional_space_count += (value_mask >> VALUEBIT_PAUSE) & VALUEPARTMASK_PAUSE;

        prior_field = digit_field;
    }
    return additional_space_count;
}

static void digits_format(struct parse_buffer * parse_in, char * formatted_digits, enum TelephoneFormat format_type,
                          int calling_code, int area_code, int prefix, int subscriber, int extension, int letters,
                          int pause_confirm) {
    int digit_index;
    int text_index = 0;
    int prior_field = FIELD_INVALID;
    int add_whitespace_next = 0;
    int add_confirm_next = 0;
    int add_pause_next = 0;
    int text_index_prior;
    for(digit_index = 0; digit_index < parse_in->digit_pos_next; ++digit_index) {
        char digit_value        = parse_in->digit_values[digit_index];
        uint value_mask         = parse_in->digit_valuesmask[digit_index];
        int  digit_field        = parse_in->digit_fields[digit_index];
        int  field_changed      = prior_field != digit_field;

        if (format_type != TFORM_DIGITS_ONLY)
            add_whitespace_next = (value_mask & VALUEMASK_WHITESPACE) == VALUEMASK_WHITESPACE;

        if (pause_confirm) {
            add_confirm_next = (value_mask & VALUEMASK_CONFIRM) == VALUEMASK_CONFIRM;
            add_pause_next = (value_mask >> VALUEBIT_PAUSE) & VALUEPARTMASK_PAUSE;
        }

        if (letters)
            value_mask &= VALUEMASK_LETTER;
        else
            value_mask = 0;

        text_index_prior = text_index;
        switch (digit_field) {
            case FIELD_CALLING_CODE:
                if (calling_code) {
                    if (field_changed && format_type != TFORM_DIGITS_ONLY) {
                        formatted_digits[text_index++] = '+';
                    }
                    formatted_digits[text_index++] = digit_value_to_text(digit_value, value_mask);
                    prior_field = digit_field;
                }
            break;
            case FIELD_NANP_NPAC:
                if (area_code) {
                    if (field_changed) {
                        switch (format_type) {
                            case TFORM_INTERNATIONAL:
                                if (prior_field != FIELD_INVALID)
                                    formatted_digits[text_index++] = ' ';
                            break;
                            case TFORM_DOMESTIC:
                                if (prior_field != FIELD_INVALID)
                                    formatted_digits[text_index++] = ' ';
                                formatted_digits[text_index++] = '(';
                            break;
                            default:break;
                        }
                    }
                    formatted_digits[text_index++] = digit_value_to_text(digit_value, value_mask);
                    prior_field = digit_field;
                }
            break;
            case FIELD_NANP_COC:
                if (prefix) {
                    if (field_changed) {
                        switch (format_type) {
                            case TFORM_INTERNATIONAL:
                                if (prior_field != FIELD_INVALID)
                                    formatted_digits[text_index++] = ' ';
                            break;
                            case TFORM_DOMESTIC:
                                if (prior_field == FIELD_NANP_NPAC)
                                    formatted_digits[text_index++] = ')';
                                if (prior_field != FIELD_INVALID)
                                    formatted_digits[text_index++] = ' ';
                            break;
                            default:break;
                        }
                    }
                    formatted_digits[text_index++] = digit_value_to_text(digit_value, value_mask);
                    prior_field = digit_field;
                }
            break;
            case FIELD_NANP_SUB:
                if (subscriber) {
                    if (field_changed) {
                        switch (format_type) {
                            case TFORM_INTERNATIONAL:
                                if (prior_field != FIELD_INVALID)
                                    formatted_digits[text_index++] = ' ';
                            break;
                            case TFORM_DOMESTIC:
                                formatted_digits[text_index++] = '-';
                            break;
                            default:break;
                        }
                    }
                    formatted_digits[text_index++] = digit_value_to_text(digit_value, value_mask);
                    prior_field = digit_field;
                }
            break;
            case FIELD_FINLAND_AREA_CODE:
                if (area_code) {
                    if (field_changed && format_type != TFORM_DIGITS_ONLY && prior_field != FIELD_INVALID) {
                        formatted_digits[text_index++] = ' ';
                    }
                    formatted_digits[text_index++] = digit_value_to_text(digit_value, value_mask);
                    prior_field = digit_field;
                }
            break;
            case FIELD_FINLAND_SUB1:
                if (subscriber) {
                    if (field_changed && format_type != TFORM_DIGITS_ONLY) {
                        formatted_digits[text_index++] = ' ';
                    }
                    formatted_digits[text_index++] = digit_value_to_text(digit_value, value_mask);
                    prior_field = digit_field;
                }
            break;
            case FIELD_FINLAND_SUB2:
                if (subscriber) {
                    if (field_changed && format_type != TFORM_DIGITS_ONLY) {
                        formatted_digits[text_index++] = ' ';
                    }
                    formatted_digits[text_index++] = digit_value_to_text(digit_value, value_mask);
                    prior_field = digit_field;
                }
            break;
            case FIELD_FINLAND_SUB3:
                if (subscriber) {
                    if (field_changed && format_type != TFORM_DIGITS_ONLY) {
                        formatted_digits[text_index++] = ' ';
                    }
                    formatted_digits[text_index++] = digit_value_to_text(digit_value, value_mask);
                    prior_field = digit_field;
                }
            break;
            case FIELD_UK_AREA_CODE:
                if (area_code) {
                    if (field_changed) {
                        switch (format_type) {
                            case TFORM_INTERNATIONAL:
                                if (prior_field != FIELD_INVALID)
                                    formatted_digits[text_index++] = ' ';
                            break;
                            case TFORM_DOMESTIC:
                                formatted_digits[text_index++] = '(';
                                formatted_digits[text_index++] = '0';
                            break;
                            default:break;
                        }
                    }
                    formatted_digits[text_index++] = digit_value_to_text(digit_value, value_mask);
                    prior_field = digit_field;
                }
            break;
            case FIELD_UK_SUB1:
                if (subscriber) {
                    if (field_changed) {
                        switch (format_type) {
                            case TFORM_INTERNATIONAL:
                                if (prior_field != FIELD_INVALID)
                                    formatted_digits[text_index++] = ' ';
                            break;
                            case TFORM_DOMESTIC:
                                formatted_digits[text_index++] = ')';
                                if (prior_field != FIELD_INVALID)
                                    formatted_digits[text_index++] = ' ';
                            break;
                            default:break;
                        }
                    }
                    formatted_digits[text_index++] = digit_value_to_text(digit_value, value_mask);
                    prior_field = digit_field;
                }
            break;
            case FIELD_UK_SUB2:
                if (subscriber) {
                    if (field_changed && format_type != TFORM_DIGITS_ONLY) {
                        formatted_digits[text_index++] = ' ';
                    }
                    formatted_digits[text_index++] = digit_value_to_text(digit_value, value_mask);
                    prior_field = digit_field;
                }
            break;
            case FIELD_AUSTRALIA_AREA_CODE:
                if (area_code) {
                    if (field_changed) {
                        switch (format_type) {
                            case TFORM_INTERNATIONAL:
                                if (prior_field != FIELD_INVALID)
                                    formatted_digits[text_index++] = ' ';
                            break;
                            case TFORM_DOMESTIC:
                                formatted_digits[text_index++] = '(';
                                formatted_digits[text_index++] = '0';
                            break;
                            default:break;
                        }
                    }
                    formatted_digits[text_index++] = digit_value_to_text(digit_value, value_mask);
                    prior_field = digit_field;
                }
            break;
            case FIELD_AUSTRALIA_SUB1:
                if (subscriber) {
                    if (field_changed) {
                        switch (format_type) {
                            case TFORM_INTERNATIONAL:
                                if (prior_field != FIELD_INVALID)
                                    formatted_digits[text_index++] = ' ';
                            break;
                            case TFORM_DOMESTIC:
                                formatted_digits[text_index++] = ')';
                                if (prior_field != FIELD_INVALID)
                                    formatted_digits[text_index++] = ' ';
                            break;
                            default:break;
                        }
                    }
                    formatted_digits[text_index++] = digit_value_to_text(digit_value, value_mask);
                    prior_field = digit_field;
                }
            break;
            case FIELD_AUSTRALIA_SUB2:
                if (subscriber) {
                    if (field_changed && format_type != TFORM_DIGITS_ONLY) {
                        formatted_digits[text_index++] = ' ';
                    }
                    formatted_digits[text_index++] = digit_value_to_text(digit_value, value_mask);
                    prior_field = digit_field;
                }
            break;
            case FIELD_RUSSIA_AREA_CODE:
                if (area_code) {
                    if (field_changed) {
                        switch (format_type) {
                            case TFORM_INTERNATIONAL:
                                if (prior_field != FIELD_INVALID)
                                    formatted_digits[text_index++] = ' ';
                            break;
                            case TFORM_DOMESTIC:
                                if (prior_field != FIELD_INVALID)
                                    formatted_digits[text_index++] = '-';
                            break;
                            default:break;
                        }
                    }
                    formatted_digits[text_index++] = digit_value_to_text(digit_value, value_mask);
                    prior_field = digit_field;
                }
            break;
            case FIELD_RUSSIA_ZONE_CODE:
                if (prefix) {
                    if (field_changed) {
                        switch (format_type) {
                            case TFORM_INTERNATIONAL:
                                if (prior_field != FIELD_INVALID)
                                    formatted_digits[text_index++] = ' ';
                            break;
                            case TFORM_DOMESTIC:
                                formatted_digits[text_index++] = '-';
                            break;
                            default:break;
                        }
                    }
                    formatted_digits[text_index++] = digit_value_to_text(digit_value, value_mask);
                    prior_field = digit_field;
                }
            break;
            case FIELD_RUSSIA_SUB1:
                if (subscriber) {
                    if (field_changed) {
                        switch (format_type) {
                            case TFORM_INTERNATIONAL:
                                if (prior_field != FIELD_INVALID)
                                    formatted_digits[text_index++] = ' ';
                            break;
                            case TFORM_DOMESTIC:
                                formatted_digits[text_index++] = '-';
                            break;
                            default:break;
                        }
                    }
                    formatted_digits[text_index++] = digit_value_to_text(digit_value, value_mask);
                    prior_field = digit_field;
                }
            break;
            case FIELD_RUSSIA_SUB2:
                if (subscriber) {
                    if (field_changed) {
                        switch (format_type) {
                            case TFORM_INTERNATIONAL:
                                if (prior_field != FIELD_INVALID)
                                    formatted_digits[text_index++] = ' ';
                            break;
                            case TFORM_DOMESTIC:
                                formatted_digits[text_index++] = '-';
                            break;
                            default:break;
                        }
                    }
                    formatted_digits[text_index++] = digit_value_to_text(digit_value, value_mask);
                    prior_field = digit_field;
                }
            break;
            case FIELD_JAPAN_AREA_CODE:
                if (area_code) {
                    if (field_changed) {
                        switch (format_type) {
                            case TFORM_INTERNATIONAL:
                                if (prior_field != FIELD_INVALID)
                                    formatted_digits[text_index++] = ' ';
                            break;
                            case TFORM_DOMESTIC:
                                if (prior_field != FIELD_INVALID)
                                    formatted_digits[text_index++] = '-';
                                formatted_digits[text_index++] = '0';
                            break;
                            default:break;
                        }
                    }
                    formatted_digits[text_index++] = digit_value_to_text(digit_value, value_mask);
                    prior_field = digit_field;
                }
            break;
            case FIELD_JAPAN_SUB1:
                if (subscriber) {
                    if (field_changed) {
                        switch (format_type) {
                            case TFORM_INTERNATIONAL:
                                if (prior_field != FIELD_INVALID)
                                    formatted_digits[text_index++] = ' ';
                            break;
                            case TFORM_DOMESTIC:
                                formatted_digits[text_index++] = '-';
                            break;
                            default:break;
                        }
                    }
                    formatted_digits[text_index++] = digit_value_to_text(digit_value, value_mask);
                    prior_field = digit_field;
                }
            break;
            case FIELD_JAPAN_SUB2:
                if (subscriber) {
                    if (field_changed) {
                        switch (format_type) {
                            case TFORM_INTERNATIONAL:
                                if (prior_field != FIELD_INVALID)
                                    formatted_digits[text_index++] = ' ';
                            break;
                            case TFORM_DOMESTIC:
                                formatted_digits[text_index++] = '-';
                            break;
                            default:break;
                        }
                    }
                    formatted_digits[text_index++] = digit_value_to_text(digit_value, value_mask);
                    prior_field = digit_field;
                }
            break;
            case FIELD_INDIA_AREA_CODE:
                if (area_code) {
                    if (field_changed && format_type != TFORM_DIGITS_ONLY && prior_field != FIELD_INVALID) {
                        formatted_digits[text_index++] = ' ';
                    }
                    formatted_digits[text_index++] = digit_value_to_text(digit_value, value_mask);
                    prior_field = digit_field;
                }
            break;
            case FIELD_INDIA_SUB1:
            case FIELD_INDIA_SUB2:
                if (subscriber) {
                    if (field_changed && format_type != TFORM_DIGITS_ONLY && prior_field != FIELD_INVALID) {
                        formatted_digits[text_index++] = ' ';
                    }
                    formatted_digits[text_index++] = digit_value_to_text(digit_value, value_mask);
                    prior_field = digit_field;
                }
            break;
            case FIELD_FRANCE_AREA_CODE:
                if (area_code) {
                    if (field_changed) {
                        switch (format_type) {
                            case TFORM_INTERNATIONAL:
                                if (prior_field != FIELD_INVALID)
                                    formatted_digits[text_index++] = ' ';
                            break;
                            case TFORM_DOMESTIC:
                                formatted_digits[text_index++] = ' ';
                                formatted_digits[text_index++] = '0';
                            break;
                            default:break;
                        }
                    }
                    formatted_digits[text_index++] = digit_value_to_text(digit_value, value_mask);
                    prior_field = digit_field;
                }
            break;
            case FIELD_FRANCE_SUB:
                if (subscriber) {
                    if (format_type != TFORM_DIGITS_ONLY && prior_field != FIELD_INVALID && digit_index % 2 == 1)
                        formatted_digits[text_index++] = ' ';
                    formatted_digits[text_index++] = digit_value_to_text(digit_value, value_mask);
                    prior_field = digit_field;
                }
            break;
            case FIELD_ITALY_AREA_CODE:
                if (area_code) {
                    if (field_changed && format_type != TFORM_DIGITS_ONLY && prior_field != FIELD_INVALID) {
                        formatted_digits[text_index++] = ' ';
                    }
                    formatted_digits[text_index++] = digit_value_to_text(digit_value, value_mask);
                    prior_field = digit_field;
                }
            break;
            case FIELD_ITALY_EX_CODE:
                if (prefix) {
                    if (field_changed && format_type != TFORM_DIGITS_ONLY && prior_field != FIELD_INVALID) {
                        formatted_digits[text_index++] = ' ';
                    }
                    formatted_digits[text_index++] = digit_value_to_text(digit_value, value_mask);
                    prior_field = digit_field;
                }
            break;
            case FIELD_ITALY_SUB1:
            case FIELD_ITALY_SUB2:
                if (subscriber) {
                    if (field_changed && format_type != TFORM_DIGITS_ONLY && prior_field != FIELD_INVALID) {
                        formatted_digits[text_index++] = ' ';
                    }
                    formatted_digits[text_index++] = digit_value_to_text(digit_value, value_mask);
                    prior_field = digit_field;
                }
            break;
            case FIELD_EXTENSION:
                if (extension) {
                    if (field_changed && prior_field != FIELD_INVALID) {
                        if (format_type == TFORM_DIGITS_ONLY) {
                            if (pause_confirm)
                                formatted_digits[text_index++] = ';';
                        } else {
                            formatted_digits[text_index++] = ' ';
                            formatted_digits[text_index++] = 'x';
                        }
                    }
                    if (format_type != TFORM_DIGITS_ONLY || digit_value != DIGIT_SPECIAL)
                        formatted_digits[text_index++] = digit_value_to_text(digit_value, value_mask);
                    prior_field = digit_field;
                }
            break;
            case FIELD_DIGITS_QUALIFIER:
            break;
            case FIELD_DIGITS:
                if (format_type != TFORM_DIGITS_ONLY || digit_value != DIGIT_SPECIAL)
                    formatted_digits[text_index++] = digit_value_to_text(digit_value, value_mask);
                prior_field = digit_field;
            break;
            default:
                if (field_changed) {
                    formatted_digits[text_index++] = '?';
                }
                formatted_digits[text_index++] = digit_value_to_text(digit_value, value_mask);
            break;
        }

        if (text_index != text_index_prior) {
            if (add_whitespace_next) {
                formatted_digits[text_index++] = ' ';
            }

            if (add_confirm_next) {
                formatted_digits[text_index++] = ';';
            }

            if (add_pause_next)
                while(add_pause_next--) {
                    formatted_digits[text_index++] = ',';
                }
        }
    }

    if (text_index != 0 &&
        parse_in->parse_state != STATE_DIGITS &&
        parse_in->parse_state != STATE_EXTENSION_START &&
        parse_in->parse_state != STATE_EXTENSION_DIGITS) {
        formatted_digits[text_index++] = '^';
    }

    formatted_digits[text_index++] = '\0';
}

static struct digit_letter digit_from_char(char digit_char) {
    struct digit_letter digit_letter;

    digit_letter.value = DIGIT_SPECIAL;
    digit_letter.letter_index = 0;
    digit_letter.invalid_char = 0;

    switch (digit_char) {
        case '0':
            digit_letter.value = 0;
        break;
        case '1':
            digit_letter.value = 1;
        break;
        case '2':
            digit_letter.value = 2;
        break;
        case '3':
            digit_letter.value = 3;
        break;
        case '4':
            digit_letter.value = 4;
        break;
        case '5':
            digit_letter.value = 5;
        break;
        case '6':
            digit_letter.value = 6;
        break;
        case '7':
            digit_letter.value = 7;
        break;
        case '8':
            digit_letter.value = 8;
        break;
        case '9':
            digit_letter.value = 9;
        break;
        case '*':
            digit_letter.value = DIGIT_STAR;
        break;
        case '#':
            digit_letter.value = DIGIT_POUND;
        break;
        case '+':
            digit_letter.value = DIGIT_PLUS;
        break;
        case '/':
             // Slash (/) must only be counted as whitespace for non-digits mode for the specific formats that use slash as a
             // separator.  Because of slash's international meaning, it should not be counted as whitespace when parsing an
             // international format.
            digit_letter.value = DIGIT_SLASH;
        break;
        case ',':
        case 'p':
            digit_letter.value = DIGIT_PAUSE;
        break;
        case ';':
        case 'w':
            digit_letter.value = DIGIT_CONFIRM;
        break;
        case ' ':
        case '-':
        case '.':
        case '(':
        case ')':
        case '[':
        case ']':
        case '\\':
            digit_letter.value = DIGIT_SPECIAL;
        break;
        case 'A':
            digit_letter.value = 2;
            digit_letter.letter_index = 1;
        break;
        case 'B':
            digit_letter.value = 2;
            digit_letter.letter_index = 2;
        break;
        case 'C':
            digit_letter.value = 2;
            digit_letter.letter_index = 3;
        break;
        case 'D':
            digit_letter.value = 3;
            digit_letter.letter_index = 1;
        break;
        case 'E':
            digit_letter.value = 3;
            digit_letter.letter_index = 2;
        break;
        case 'F':
            digit_letter.value = 3;
            digit_letter.letter_index = 3;
        break;
        case 'G':
            digit_letter.value = 4;
            digit_letter.letter_index = 1;
        break;
        case 'H':
            digit_letter.value = 4;
            digit_letter.letter_index = 2;
        break;
        case 'I':
            digit_letter.value = 4;
            digit_letter.letter_index = 3;
        break;
        case 'J':
            digit_letter.value = 5;
            digit_letter.letter_index = 1;
        break;
        case 'K':
            digit_letter.value = 5;
            digit_letter.letter_index = 2;
        break;
        case 'L':
            digit_letter.value = 5;
            digit_letter.letter_index = 3;
        break;
        case 'M':
            digit_letter.value = 6;
            digit_letter.letter_index = 1;
        break;
        case 'N':
            digit_letter.value = 6;
            digit_letter.letter_index = 2;
        break;
        case 'O':
            digit_letter.value = 6;
            digit_letter.letter_index = 3;
        break;
        case 'P':
            digit_letter.value = 7;
            digit_letter.letter_index = 1;
        break;
        case 'Q':
            digit_letter.value = 7;
            digit_letter.letter_index = 2;
        break;
        case 'R':
            digit_letter.value = 7;
            digit_letter.letter_index = 3;
        break;
        case 'S':
            digit_letter.value = 7;
            digit_letter.letter_index = 4;
        break;
        case 'T':
            digit_letter.value = 8;
            digit_letter.letter_index = 1;
        break;
        case 'U':
            digit_letter.value = 8;
            digit_letter.letter_index = 2;
        break;
        case 'V':
            digit_letter.value = 8;
            digit_letter.letter_index = 3;
        break;
        case 'W':
            digit_letter.value = 9;
            digit_letter.letter_index = 1;
        break;
        case 'X':
            digit_letter.value = 9;
            digit_letter.letter_index = 2;
        break;
        case 'Y':
            digit_letter.value = 9;
            digit_letter.letter_index = 3;
        break;
        case 'Z':
            digit_letter.value = 9;
            digit_letter.letter_index = 4;
        break;
        default:
            digit_letter.invalid_char = digit_char;
        break;
    }

    return digit_letter;
}

static void report_byte_format_error(struct parse_buffer * parse_in, int byte_index) {
    ereport(
        ERROR,
        (   errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
            errmsg("Telephone error type %d, ID %d while loading the number in byte %d.", parse_in->error_code, parse_in->code_id,
                byte_index),
            errhint("Are you using older telephone code to read data created by newer telephone code?  Load the bytea cast and cast"
                "it to examine the raw data.")
        )
    );
}

/*int service_query_nanp(struct parse_buffer * parse_in) {
    switch (parse_in->digit_values[1]) {
        case 8:
            if ((
                parse_in->digit_values[2] == 0 ||
                parse_in->digit_values[2] == 8 ||
                parse_in->digit_values[2] == 7 ||
                parse_in->digit_values[2] == 6 ||
                parse_in->digit_values[2] == 5 ||
                parse_in->digit_values[2] == 4) &&
                parse_in->digit_values[2] == parse_in->digit_values[3])
                return TSERV_TOLLFREE;
        case 9:
            if (parse_in->digit_values[2] == 0 && parse_in->digit_values[3] == 0)
                return TSERV_CHARGE;
    }
    return TSERV_UNKNOWN_UNKNOWN;
}*/

static int service_query_russia(struct parse_buffer * parse_in) {
    switch (parse_in->digit_values[1] * 100 + parse_in->digit_values[2] * 10 + parse_in->digit_values[3]) {
        case 301:
        case 302:
        case 341:
        case 342:
        case 343:
        case 345:
        case 346:
        case 347:
        case 349:
        case 351:
        case 352:
        case 353:
        case 365:
        case 381:
        case 382:
        case 383:
        case 384:
        case 385:
        case 388:
        case 390:
        case 391:
        case 394:
        case 395:
        case 401:
        case 411:
        case 413:
        case 415:
        case 416:
        case 421:
        case 423:
        case 424:
        case 426:
        case 427:
        case 471:
        case 472:
        case 473:
        case 474:
        case 475:
        case 481:
        case 482:
        case 483:
        case 484:
        case 485:
        case 486:
        case 487:
        case 491:
        case 492:
        case 493:
        case 494:
        case 495:
        case 496:
        case 498:
        case 499:
        case 811:
        case 812:
        case 813:
        case 814:
        case 815:
        case 816:
        case 817:
        case 818:
        case 820:
        case 821:
        case 831:
        case 833:
        case 834:
        case 835:
        case 836:
        case 841:
        case 842:
        case 843:
        case 844:
        case 845:
        case 846:
        case 847:
        case 848:
        case 851:
        case 855:
        case 861:
        case 862:
        case 863:
        case 865:
        case 866:
        case 867:
        case 869:
        case 871:
        case 872:
        case 873:
        case 877:
        case 878:
        case 879:
            return TSERV_LAND;
        case 800:
            return TSERV_TOLLFREE;
        case 809:
            return TSERV_CHARGE;
        case 901:
        case 902:
        case 903:
        case 904:
        case 905:
        case 906:
        case 908:
        case 909:
        case 911:
        case 912:
        case 914:
        case 916:
        case 918:
        case 921:
        case 922:
        case 923:
        case 924:
        case 926:
        case 927:
        case 928:
        case 929:
        case 930:
        case 931:
        case 932:
        case 933:
        case 934:
        case 936:
        case 937:
        case 938:
        case 950:
        case 951:
        case 952:
        case 953:
        case 960:
        case 961:
        case 962:
        case 963:
        case 964:
        case 965:
        case 980:
        case 981:
        case 982:
        case 983:
        case 984:
        case 985:
        case 987:
        case 988:
        case 989:
        case 997:
            return TSERV_CELL;
    }

    // This makes some of the codes above redundant.
    if (parse_in->digit_values[1] == 3 || parse_in->digit_values[1] == 4)
        return TSERV_LAND;

    return TSERV_UNKNOWN_UNKNOWN;
}

static enum TelephoneService service_get(struct parse_buffer * parse_in) {
    switch (parse_in->service_type) {
        case TSERV_KNOWN_UNKNOWN:
            return TSERV_KNOWN_UNKNOWN;
        case TSERV_NEED_TO_PARSE:
            return TSERV_UNKNOWN_UNKNOWN;
        case TSERV_LAND:
            return TSERV_LAND;
        case TSERV_OTHER_GEO:
            return TSERV_OTHER_GEO;
        case TSERV_LAND_OR_CELL:
            return TSERV_LAND_OR_CELL;
        case TSERV_CELL:
            return TSERV_CELL;
        case TSERV_PAGER:
            return TSERV_PAGER;
        case TSERV_VOIP:
            return TSERV_VOIP;
        case TSERV_FOLLOW_ME:
            return TSERV_FOLLOW_ME;
        case TSERV_TOLLFREE:
            return TSERV_TOLLFREE;
        case TSERV_SHARED_COST:
            return TSERV_SHARED_COST;
        case TSERV_CALLER_COST:
            return TSERV_CALLER_COST;
        case TSERV_CHARGE:
            return TSERV_CHARGE;
        case TSERV_CARRIER_SERVICES_INTRA:
            return TSERV_CARRIER_SERVICES_INTRA;
        case TSERV_CARRIER_SERVICES_INTERNATIONAL:
            return TSERV_CARRIER_SERVICES_INTERNATIONAL;
        case TSERV_VOICEMAIL:
            return TSERV_VOICEMAIL;
        case TSERV_GOVERNMENT:
            return TSERV_GOVERNMENT;
        case TSERV_OTHER:
            return TSERV_OTHER;
        //case TSERV_QUERY_NANP:
        //    return service_query_nanp(parse_in);
        case TSERV_QUERY_RUSSIA:
            return service_query_russia(parse_in);
        default:
            return TSERV_UNKNOWN_UNKNOWN;
    }
}

static int geo_is(struct parse_buffer * parse_in) {
    switch (service_get(parse_in)) {
        case TSERV_LAND:
        case TSERV_OTHER_GEO:
        case TSERV_LAND_OR_CELL:
            return 1;
        default:
            return 0;
    }
}

PG_FUNCTION_INFO_V1(telephone_geo_is);
Datum telephone_geo_is(PG_FUNCTION_ARGS) {
    bytea   *vlena      = PG_GETARG_BYTEA_P(0);
    char    *phone_hex  = VARDATA(vlena);
    int     hex_len     = VARSIZE(vlena) - VARHDRSZ;
    int     is_digits_only;
    int     is_mask;

    struct parse_buffer parse_out;
    parse_out.parse_state       = STATE_CALLING_CODE_START;  // Only valid if !is_digits_only
    parse_out.digit_pos_next    = 0;
    parse_out.error_code        = ERROR_PARSE_STATE_OR_OTHER;
    parse_out.service_type      = TSERV_NEED_TO_PARSE;
    //memset(parse_out.digit_values, 0, sizeof(parse_out.digit_values));
    //memset(parse_out.digit_valuesmask, 0, sizeof(parse_out.digit_valuesmask));
    //memset(parse_out.digit_fields, 0, sizeof(parse_out.digit_fields));
    parse_out.code_id = 0;

    is_digits_only = ((phone_hex[0] >> 4) & 0xF) == DIGIT_BIN_SPECIAL;
    if (is_digits_only)
        PG_RETURN_NULL();

    is_mask = phone_hex[hex_len - 1] == DIGIT_BIN_HEX_SPECIAL_SPECIAL;

    if (is_mask)
        hex_len = hex_len / 3; // Remove mask from digits hex len.

    {
        struct digit_letter digit_letter;
        int last_hex_index = hex_len - 1;
        int hex_index;
        digit_letter.letter_index = 0;
        digit_letter.invalid_char = 0;
        for(hex_index = 0; hex_index < hex_len; ++hex_index) {
            int hex_value = phone_hex[hex_index];
            int nibble1 = ((hex_value >> 4) & 0xF) + DIGIT_BIN_OFFSET;
            int nibble2 = (hex_value & 0xF) + DIGIT_BIN_OFFSET;

            digit_letter.value = nibble1;
            parse_digit(digit_letter, &parse_out);
            if (parse_out.parse_state == STATE_ERROR)
                break;
            if (parse_out.service_type != TSERV_NEED_TO_PARSE)
                PG_RETURN_BOOL(geo_is(&parse_out));

            // Do not read the filler nibble.
            if (hex_index == last_hex_index && nibble2 == DIGIT_SPECIAL)
                break;

            digit_letter.value = nibble2;
            parse_digit(digit_letter, &parse_out);
            if (parse_out.parse_state == STATE_ERROR)
                break;
            if (parse_out.service_type != TSERV_NEED_TO_PARSE)
                PG_RETURN_BOOL(geo_is(&parse_out));
        }

        if (parse_out.parse_state == STATE_ERROR)
            report_byte_format_error(&parse_out, hex_index);
    }

    ereport(
        ERROR,
        (   errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
            errmsg("Cannot find geo status.")
        )
    );
}

static void report_parse_error(struct parse_buffer * parse_in, char *phone_text, int text_index) {
    switch (parse_in->error_code) {
        case ERROR_CALLING_CODE_START_INVALID:
        case ERROR_AREA_CODE_START_INVALID:
        case ERROR_EXCHANGE_CODE_START_INVALID:
        case ERROR_SUBSCRIBER_NUM_START_INVALID:
        case ERROR_SUBSCRIBER_NUM_START2_INVALID:
        case ERROR_EXTENSION_START_INVALID:
        case ERROR_EXTENSION_DIGITS_INVALID:
            ereport(ERROR, (
                errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                errmsg("The character \"%c\" is not valid in this part of a telephone number.  The full text is \"%s\" and the posi"
                    "tion of the invalid character is %d.  The error type is %d and the error ID is %d.", phone_text[text_index],
                    phone_text, text_index + 1, parse_in->error_code, parse_in->code_id),
                errhint("While this character may be permitted in some parts of a telephone number, it is not permitted in this par"
                    "t.")));
        case ERROR_CALLING_CODE_DIGITS_INVALID:
        case ERROR_AREA_CODE_DIGITS_INVALID:
        case ERROR_EXCHANGE_CODE_DIGITS_INVALID:
        case ERROR_SUBSCRIBER_NUM_DIGITS_INVALID:
            ereport(ERROR, (
                errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                errmsg("The character \"%c\" is not valid in this part of a telephone number.  The full text is \"%s\" and the posi"
                    "tion of the invalid character is %d.  The error type is %d and the error ID is %d.", phone_text[text_index],
                    phone_text, text_index + 1, parse_in->error_code, parse_in->code_id),
                errhint("While this character may be permitted in some parts of a telephone number, it is not permitted in this par"
                    "t.  Formatting characters are only permitted between parts of a telephone number.")));
        case ERROR_INVALID_CHAR:
            ereport(ERROR, (
                errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                errmsg("The character \"%c\" is not valid in any part of a telephone number.  The full text is \"%s\" and the posit"
                    "ion of the invalid character is %d.", phone_text[text_index], phone_text, text_index + 1),
                errhint("Valid characters include 0-9, A-Z, star (*), hash (#), plus (+), slash (/), pause (, or p), confirm (; or "
                    "w), or formatting (space, -, ., (, ), [, ], \\).")));
        case ERROR_PARSE_STATE_OR_OTHER:
            ereport(ERROR, (
                errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                errmsg("Text to telephone conversion error type %d, error ID %d on character %d in text \"%s\".  The code did not t"
                    "ransition correctly when switching between telephone parts.", parse_in->error_code, parse_in->code_id,
                    text_index + 1, phone_text),
                errhint("The code no longer considers the telephone number to be valid at this character, but prior characters may "
                    "built a telephone number that is incorrect and the problem was only detected until it got to this character.  "
                    "Use the digits mode to store this telephone number by removing the + until the code is fixed.")));
        case ERROR_DIGITS_INVALID:
            ereport(ERROR, (
                errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                errmsg("The character \"%c\" is not valid within a telephone number.  The full text is \"%s\" and the position of t"
                    "he invalid character is %d.", phone_text[text_index], phone_text, text_index + 1),
                errhint("Valid characters include 0-9, A-Z, star (*), hash (#), slash (/), pause (, or p), confirm (; or w), or for"
                    "matting (space, -, ., (, ), [, ], \\).")));
        case ERROR_NEED_LENGTH_CODE:
            ereport(ERROR, (
                errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                errmsg("The code does not know the length of the telephone number for this part of the dialing plan.  The full text"
                    " is \"%s\" and the character position is %d.", phone_text, text_index + 1),
                errhint("If this is a correct telephone number, find the dialing plan documentation and update the code.  Use the d"
                    "igits mode to store this telephone number by removing the + until the code is updated.")));
        case ERROR_AREA_CODE_NOT_FOUND:
            ereport(ERROR, (
                errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                errmsg("The area code was not found.  The full text is \"%s\" and the character position is %d.", phone_text,
                    text_index + 1),
                errhint("Check the area code.  If it is correct, update the code.  Use the digits mode to store this telephone numb"
                    "er by removing the + until the code is updated.")));
        case ERROR_NUMBER_OBSOLETE:
            ereport(ERROR, (
                errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                errmsg("This telephone number might have been valid under an older dialing plan, but that dialing plan is now obsol"
                    "ete.  The full text is \"%s\" and the character position is %d.", phone_text, text_index + 1),
                errhint("Update the telephone number to the current dialing plan.  If you must store an obsolete number, you can us"
                    "e the digits mode to store this number by removing the +.")));
        case ERROR_CALLING_CODE_NEED_CODE:
            ereport(ERROR, (
                errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                errmsg("The dialing plan for this calling code has not been implemented in the code.  The full text is \"%s\" and t"
                    "he character position is %d.", phone_text, text_index + 1),
                errhint("Use the digits mode to store this telephone number by removing the +.")));
        default:
            ereport(ERROR, (
                errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                errmsg("Text to telephone conversion error type %d, error ID %d on character %d in text \"%s\".",
                       parse_in->error_code, parse_in->code_id, text_index + 1, phone_text),
                errhint("The code no longer considers the telephone number to be valid at this character, but prior characters may "
                    "built a telephone number that is incorrect and the problem was only detected until it got to this character."))
                    );
    }
}

static bytea * telephone_parse_buffer_to_bytea(struct parse_buffer *parse_in, int is_mask, int digit_end, int digit_start) {
    bytea  *result;
    //int is_digits_format = parse_in.parse_state == STATE_START || parse_in.parse_state == STATE_DIGITS;
    int hex_len = digit_end - digit_start;

    // Add for odd nibble.
    if (hex_len % 2 != 0)
        hex_len++;

    // Add for mask indicator.
    if (is_mask) {
        hex_len = hex_len / 2 + parse_in->digit_pos_next + 1;
    } else {
        hex_len = hex_len / 2;
    }

    result = palloc0(hex_len + VARHDRSZ);
    {
        char * hex_array = VARDATA(result);
        char nibbles;
        int on_nibble2 = 0;
        int hex_index = 0;
        int digit_index;
        SET_VARSIZE(result, hex_len + VARHDRSZ);
        for(digit_index = digit_start; digit_index < digit_end; ++digit_index) {
            if (on_nibble2) {
                nibbles = nibbles | (parse_in->digit_values[digit_index] - DIGIT_BIN_OFFSET);
                hex_array[hex_index++] = nibbles;
                on_nibble2 = 0;
            } else {
                nibbles = ((parse_in->digit_values[digit_index] - DIGIT_BIN_OFFSET) << 4);
                on_nibble2 = 1;
            }
        }
        if (on_nibble2) {
            hex_array[hex_index++] = nibbles;
            on_nibble2 = 0;
        }

        if (is_mask) {
            for(digit_index = 0; digit_index < parse_in->digit_pos_next; ++digit_index) {
                hex_array[hex_index++] = parse_in->digit_valuesmask[digit_index];
            }
            hex_array[hex_index++] = DIGIT_BIN_HEX_SPECIAL_SPECIAL;
        }

        if (hex_index != hex_len) {
            ereport(
                ERROR,
                (   errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
                    errmsg("The data was not allocated correctly or not written correctly.")
                )
            );
        }
    }

    return result;
}

static bytea * telephone_char_to_bytea(char *phone_text) {
    int     is_mask = 0;
    struct  parse_buffer parse_out;

    parse_out.parse_state = STATE_START;
    parse_out.digit_pos_next = 0;
    parse_out.error_code = ERROR_PARSE_STATE_OR_OTHER;
    parse_out.service_type = TSERV_NEED_TO_PARSE;
    //memset(parse_out.digit_values, 0, sizeof(parse_out.digit_values));
    //memset(parse_out.digit_valuesmask, 0, sizeof(parse_out.digit_valuesmask));
    //memset(parse_out.digit_fields, 0, sizeof(parse_out.digit_fields));
    parse_out.code_id = 0;

    {
        int text_index;
        char current_char;
        struct digit_letter digit_letter;
        for(text_index = 0; phone_text[text_index] != 0; ++text_index) {
            if (parse_out.digit_pos_next >= MAX_DIGITS - MAX_STORAGE_HEADER)
                ereport(ERROR, (
                    errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                    errmsg("A telephone number must not consist of more than %u characters.  The text is \"%s\".",
                           MAX_DIGITS - MAX_STORAGE_HEADER, phone_text),
                    errhint("Only store actual telephone numbers and extensions.")));

            current_char = phone_text[text_index];
            digit_letter = digit_from_char(current_char);
            parse_digit(digit_letter, &parse_out);
            if (parse_out.parse_state == STATE_ERROR) {
                if (digit_letter.invalid_char != '^' || phone_text[text_index + 1] != 0)
                    report_parse_error(&parse_out, phone_text, text_index);
            }
        }
    }

    if (parse_out.parse_state > STATE_DIGITS && parse_out.parse_state < STATE_EXTENSION_START)
        ereport(ERROR, (
            errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
            errmsg("The telephone number is not complete.  The text is \"%s\".", phone_text),
            errhint("Add the missing digits or use the digits mode to store this telephone number by removing the +.")));

    {
        int digit_index;
        for(digit_index = 0; digit_index < parse_out.digit_pos_next; ++digit_index) {
            if (parse_out.digit_valuesmask[digit_index] != 0)
                is_mask = 1;
        }
    }

    return telephone_parse_buffer_to_bytea(&parse_out, is_mask, parse_out.digit_pos_next, 0);
}

static struct parse_buffer telephone_bytea_to_parse_buffer(bytea *vlena) {
    char    *phone_hex  = VARDATA(vlena);
    int     hex_len     = VARSIZE(vlena) - VARHDRSZ;
    int     is_digits_only;
    int     is_mask;
    int     digit_valuesmask_index;
    struct parse_buffer parse_out;

    parse_out.parse_state = STATE_START;
    parse_out.digit_pos_next = 0;
    parse_out.error_code = ERROR_PARSE_STATE_OR_OTHER;
    parse_out.service_type = TSERV_NEED_TO_PARSE;
    //memset(parse_out.digit_values, 0, sizeof(parse_out.digit_values));
    //memset(parse_out.digit_valuesmask, 0, sizeof(parse_out.digit_valuesmask));
    //memset(parse_out.digit_fields, 0, sizeof(parse_out.digit_fields));
    parse_out.code_id = 0;

    is_digits_only = ((phone_hex[0] >> 4) & 0xF) == DIGIT_BIN_SPECIAL;
    is_mask = phone_hex[hex_len - 1] == DIGIT_BIN_HEX_SPECIAL_SPECIAL;

    if (is_mask) {
        hex_len = hex_len / 3; // Remove mask from digits hex len.
        digit_valuesmask_index = hex_len;
    }

    if (is_digits_only) {
        parse_out.parse_state = STATE_DIGITS_IGNORE_QUALIFIER;
    } else {
        parse_out.parse_state = STATE_CALLING_CODE_START;
    }

    {
        struct digit_letter digit_letter;
        int last_hex_index = hex_len - 1;
        int hex_index;
        digit_letter.letter_index = 0;
        digit_letter.invalid_char = 0;
        readhex:
        for(hex_index = 0; hex_index < hex_len; ++hex_index) {
            int hex_value = phone_hex[hex_index];
            int nibble1 = ((hex_value >> 4) & 0xF) + DIGIT_BIN_OFFSET;
            int nibble2 = (hex_value & 0xF) + DIGIT_BIN_OFFSET;

            digit_letter.value = nibble1;
            parse_digit(digit_letter, &parse_out);
            if (is_mask) {
                if (parse_out.digit_pos_next == 0) {
                    digit_valuesmask_index++; // Skip over digits mode indicator.
                } else {
                    parse_out.digit_valuesmask[parse_out.digit_pos_next - 1] = phone_hex[digit_valuesmask_index++];
                }
            }
            if (parse_out.parse_state == STATE_ERROR)
                break;

            // Do not read the filler nibble.
            if (hex_index == last_hex_index && nibble2 == DIGIT_SPECIAL)
                break;

            digit_letter.value = nibble2;
            parse_digit(digit_letter, &parse_out);
            if (is_mask) {
                parse_out.digit_valuesmask[parse_out.digit_pos_next - 1] = phone_hex[digit_valuesmask_index++];
            }
            if (parse_out.parse_state == STATE_ERROR)
                break;
        }

        // If a different version of telephone wrote data that is incompatible with this version, retry the calling code data with
        // the digits mode.
        if (parse_out.parse_state == STATE_ERROR && !is_digits_only) {
            is_digits_only = 1;
            parse_out.parse_state = STATE_DIGITS;
            parse_out.digit_pos_next = 0;
            parse_out.error_code = ERROR_PARSE_STATE_OR_OTHER;
            parse_out.service_type = TSERV_NEED_TO_PARSE;
            parse_out.code_id = 0;
            goto readhex;
        }

        if (parse_out.parse_state == STATE_ERROR)
            report_byte_format_error(&parse_out, hex_index);
    }

    return parse_out;
}

static char * telephone_bytea_to_char(bytea *vlena) {
    struct parse_buffer parse_out = telephone_bytea_to_parse_buffer(vlena);
    char *formatted_digits = (char *) palloc(parse_out.digit_pos_next + CALLING_CODE_FORMATTING_MAX_LEN +
        additional_space_count(&parse_out));

    digits_format(&parse_out, formatted_digits, TFORM_INTERNATIONAL, 1, 1, 1, 1, 1, 1, 1);
    return formatted_digits;
}

/*****************************************************************************
 * Input/Output functions
 *****************************************************************************/

PG_FUNCTION_INFO_V1(telephone_in);

// String to bytea
Datum telephone_in(PG_FUNCTION_ARGS) {
    PG_RETURN_BYTEA_P(telephone_char_to_bytea(PG_GETARG_CSTRING(0)));
}

PG_FUNCTION_INFO_V1(telephone_out);

// bytea to string
Datum telephone_out(PG_FUNCTION_ARGS) {
    PG_RETURN_CSTRING(telephone_bytea_to_char(PG_GETARG_BYTEA_P(0)));
}

/*****************************************************************************
 * Type to/from text conversion routines
 *****************************************************************************/

PG_FUNCTION_INFO_V1(telephone_to_text);
Datum telephone_to_text(PG_FUNCTION_ARGS) {
    char    *formatted_digits;
    text    *return_text;

    formatted_digits = telephone_bytea_to_char(PG_GETARG_BYTEA_P(0));
    return_text = DatumGetTextP(DirectFunctionCall1(textin, CStringGetDatum(formatted_digits)));
    PG_RETURN_TEXT_P(return_text);
}

PG_FUNCTION_INFO_V1(telephone_from_text);
Datum telephone_from_text(PG_FUNCTION_ARGS) {
    text    *arg_text = PG_GETARG_TEXT_P(0);
    char    *phone_text = DatumGetCString(DirectFunctionCall1(textout, PointerGetDatum(arg_text)));
    PG_RETURN_BYTEA_P(telephone_char_to_bytea(phone_text));
}

/*****************************************************************************
 * Binary Input/Output functions
 *****************************************************************************/

PG_FUNCTION_INFO_V1(telephone_recv);

Datum telephone_recv(PG_FUNCTION_ARGS) {
    StringInfo  buf = (StringInfo) PG_GETARG_POINTER(0);
    bytea       *result;
    int         nbytes;

    nbytes = buf->len - buf->cursor;
    result = (bytea *) palloc(nbytes + VARHDRSZ);
    SET_VARSIZE(result, nbytes + VARHDRSZ);
    pq_copymsgbytes(buf, VARDATA(result), nbytes);
    PG_RETURN_BYTEA_P(result);
}

PG_FUNCTION_INFO_V1(telephone_send);

Datum telephone_send(PG_FUNCTION_ARGS) {
    bytea      *vlena = PG_GETARG_BYTEA_P_COPY(0);

    PG_RETURN_BYTEA_P(vlena);
}

/*****************************************************************************
 * Indexing functions
 *****************************************************************************/

static int32 telephone_cmp_internal(bytea *vlena1, bytea *vlena2) {
    char    *phone_hex1 = VARDATA(vlena1);
    int     hex_len1    = VARSIZE(vlena1) - VARHDRSZ;
    int     is_mask1    = phone_hex1[hex_len1 - 1] == DIGIT_BIN_HEX_SPECIAL_SPECIAL;
    char    *phone_hex2 = VARDATA(vlena2);
    int     hex_len2    = VARSIZE(vlena2) - VARHDRSZ;
    int     is_mask2    = phone_hex2[hex_len2 - 1] == DIGIT_BIN_HEX_SPECIAL_SPECIAL;
    int     result;

    if (is_mask1)
        hex_len1 = hex_len1 / 3; // Remove mask from digits hex len.

    if (is_mask2)
        hex_len2 = hex_len2 / 3; // Remove mask from digits hex len.

    result = memcmp(phone_hex1, phone_hex2, Min(hex_len1, hex_len2));

    if ((result == 0) && (hex_len1 != hex_len2))
        result = (hex_len1 < hex_len2) ? -1 : 1;

    return result;
}

PG_FUNCTION_INFO_V1(telephone_cmp);
Datum telephone_cmp(PG_FUNCTION_ARGS) {
    bytea    *vlena1 = PG_GETARG_BYTEA_P(0);
    bytea    *vlena2 = PG_GETARG_BYTEA_P(1);

    PG_RETURN_INT32(telephone_cmp_internal(vlena1, vlena2));
}

PG_FUNCTION_INFO_V1(telephone_hash);
Datum telephone_hash(PG_FUNCTION_ARGS) {
    bytea   *vlena = PG_GETARG_BYTEA_P(0);
    char    *phone_hex = VARDATA(vlena);
    int     hex_len    = VARSIZE(vlena) - VARHDRSZ;
    int     is_mask     = phone_hex[hex_len - 1] == DIGIT_BIN_HEX_SPECIAL_SPECIAL;
    Datum   result;

    if (is_mask)
        hex_len = hex_len / 3; // Remove mask from digits hex len.

    result = hash_any((unsigned char *) phone_hex, hex_len);

    /* Avoid leaking memory for toasted inputs
    PG_FREE_IF_COPY(vlena, 0); */

    PG_RETURN_DATUM(result);
}

/*****************************************************************************
 * Boolean comparisons
 *****************************************************************************/

PG_FUNCTION_INFO_V1(telephone_lt);
Datum telephone_lt(PG_FUNCTION_ARGS) {
    bytea    *vlena1 = PG_GETARG_BYTEA_P(0);
    bytea    *vlena2 = PG_GETARG_BYTEA_P(1);

    PG_RETURN_BOOL(telephone_cmp_internal(vlena1, vlena2) < 0);
}

PG_FUNCTION_INFO_V1(telephone_le);
Datum telephone_le(PG_FUNCTION_ARGS) {
    bytea    *vlena1 = PG_GETARG_BYTEA_P(0);
    bytea    *vlena2 = PG_GETARG_BYTEA_P(1);

    PG_RETURN_BOOL(telephone_cmp_internal(vlena1, vlena2) <= 0);
}

PG_FUNCTION_INFO_V1(telephone_eq);
Datum telephone_eq(PG_FUNCTION_ARGS) {
    bytea    *vlena1 = PG_GETARG_BYTEA_P(0);
    bytea    *vlena2 = PG_GETARG_BYTEA_P(1);

    PG_RETURN_BOOL(telephone_cmp_internal(vlena1, vlena2) == 0);
}

PG_FUNCTION_INFO_V1(telephone_ge);
Datum telephone_ge(PG_FUNCTION_ARGS) {
    bytea    *vlena1 = PG_GETARG_BYTEA_P(0);
    bytea    *vlena2 = PG_GETARG_BYTEA_P(1);

    PG_RETURN_BOOL(telephone_cmp_internal(vlena1, vlena2) >= 0);
}

PG_FUNCTION_INFO_V1(telephone_gt);
Datum telephone_gt(PG_FUNCTION_ARGS)
{
    bytea    *vlena1 = PG_GETARG_BYTEA_P(0);
    bytea    *vlena2 = PG_GETARG_BYTEA_P(1);

    PG_RETURN_BOOL(telephone_cmp_internal(vlena1, vlena2) > 0);
}

PG_FUNCTION_INFO_V1(telephone_ne);
Datum telephone_ne(PG_FUNCTION_ARGS) {
    bytea    *vlena1 = PG_GETARG_BYTEA_P(0);
    bytea    *vlena2 = PG_GETARG_BYTEA_P(1);

    PG_RETURN_BOOL(telephone_cmp_internal(vlena1, vlena2) != 0);
}

/*****************************************************************************
 * Aggregate functions
 *****************************************************************************/

PG_FUNCTION_INFO_V1(telephone_smaller);
Datum telephone_smaller(PG_FUNCTION_ARGS) {
   bytea *left  = PG_GETARG_BYTEA_P(0);
   bytea *right = PG_GETARG_BYTEA_P(1);
   bytea *result;

   result = telephone_cmp_internal(left, right) < 0 ? left : right;
   PG_RETURN_TEXT_P(result);
}

PG_FUNCTION_INFO_V1(telephone_larger);
Datum telephone_larger(PG_FUNCTION_ARGS) {
   bytea *left  = PG_GETARG_BYTEA_P(0);
   bytea *right = PG_GETARG_BYTEA_P(1);
   bytea *result;

   result = telephone_cmp_internal(left, right) > 0 ? left : right;
   PG_RETURN_TEXT_P(result);
}


static enum TelephoneMode mode_get(bytea *vlena) {
    char    *phone_hex  = VARDATA(vlena);

    if (((phone_hex[0] >> 4) & 0xF) == DIGIT_BIN_SPECIAL)
        return TMODE_DIGITS;
    else
        return TMODE_CALLING_CODE;
}

PG_FUNCTION_INFO_V1(telephone_to_format);
Datum telephone_to_format(PG_FUNCTION_ARGS) {
    //Telephone   *telephone      = (Telephone *) PG_GETARG_VARLENA_P(0);
    bytea       *vlena              = PG_GETARG_BYTEA_P(0);
    Oid         format_type_oid     = PG_GETARG_OID(1);
    bool        inclusive_subfields = PG_GETARG_BOOL(2);
    ArrayType   *subfields          = PG_GETARG_ARRAYTYPE_P(3);
    bool        letters             = PG_GETARG_BOOL(4);
    bool        pause_confirm       = PG_GETARG_BOOL(5);
    bool        allow_digits_mode   = PG_GETARG_BOOL(6);
    bool        tsf_digits, tsf_calling_code,
                tsf_group1, tsf_group2, tsf_group3, tsf_group4,
                tsf_subscriber, tsf_extension;
    char        *formatted_digits;
    struct parse_buffer parse_out;
    enum TelephoneFormat format_type = TFORM_INTERNATIONAL;

    init_oids_format();
    if (format_type_oid == format_oids[TFORM_INTERNATIONAL]) {
        format_type = TFORM_INTERNATIONAL;
    } else if (format_type_oid == format_oids[TFORM_DOMESTIC]) {
        format_type = TFORM_DOMESTIC;
    } else if (format_type_oid == format_oids[TFORM_DIGITS_ONLY]) {
        format_type = TFORM_DIGITS_ONLY;
    }

    if (!allow_digits_mode)
        if (mode_get(vlena) != TMODE_CALLING_CODE)
             PG_RETURN_NULL();

    tsf_digits = tsf_calling_code =
        tsf_group1 = tsf_group2 = tsf_group3 = tsf_group4 =
        tsf_subscriber = tsf_extension = inclusive_subfields == 0;

    init_oids_subfield();
    {
        Datum       subfield;
        bool        isnull;
        ArrayIterator array_iterator = array_create_iterator(subfields, 0, NULL);

        while (array_iterate(array_iterator, &subfield, &isnull)) {
            if (subfield == subfield_oids[TSF_DIGITS]) {
                tsf_digits = inclusive_subfields;
            } else if (subfield == subfield_oids[TSF_CALLING_CODE]) {
                tsf_calling_code = inclusive_subfields;
            } else if (subfield == subfield_oids[TSF_GROUP1]) {
                tsf_group1 = inclusive_subfields;
            } else if (subfield == subfield_oids[TSF_GROUP2]) {
                tsf_group2 = inclusive_subfields;
            } else if (subfield == subfield_oids[TSF_GROUP3]) {
                tsf_group3 = inclusive_subfields;
            } else if (subfield == subfield_oids[TSF_GROUP4]) {
                tsf_group4 = inclusive_subfields;
            } else if (subfield == subfield_oids[TSF_SUBSCRIBER]) {
                tsf_subscriber = inclusive_subfields;
            } else if (subfield == subfield_oids[TSF_EXTENSION]) {
                tsf_extension = inclusive_subfields;
            }
        }
        array_free_iterator(array_iterator);
    }

    parse_out = telephone_bytea_to_parse_buffer(vlena);
    formatted_digits = (char *) palloc(parse_out.digit_pos_next + CALLING_CODE_FORMATTING_MAX_LEN +
        additional_space_count(&parse_out));
    digits_format(&parse_out, formatted_digits, format_type, tsf_calling_code, tsf_group1, tsf_group2,
                  tsf_subscriber, tsf_extension, letters, pause_confirm);
    PG_RETURN_TEXT_P(cstring_to_text(formatted_digits));
}


PG_FUNCTION_INFO_V1(telephone_mode_get);
Datum telephone_mode_get(PG_FUNCTION_ARGS) {
    bytea   *vlena      = PG_GETARG_BYTEA_P(0);

    init_oids_mode();
    PG_RETURN_OID(mode_oids[mode_get(vlena)]);
}


PG_FUNCTION_INFO_V1(telephone_calling_code_get);
Datum telephone_calling_code_get(PG_FUNCTION_ARGS) {
    bytea   *vlena      = PG_GETARG_BYTEA_P(0);
    char    *phone_hex  = VARDATA(vlena);
    int     hex_len     = VARSIZE(vlena) - VARHDRSZ;
    int     is_digits_only;
    int     is_mask;

    struct parse_buffer parse_out;
    parse_out.parse_state       = STATE_CALLING_CODE_START;  // Only valid if !is_digits_only
    parse_out.digit_pos_next    = 0;
    parse_out.error_code        = ERROR_PARSE_STATE_OR_OTHER;
    parse_out.service_type      = TSERV_NEED_TO_PARSE;
    //memset(parse_out.digit_values, 0, sizeof(parse_out.digit_values));
    //memset(parse_out.digit_valuesmask, 0, sizeof(parse_out.digit_valuesmask));
    //memset(parse_out.digit_fields, 0, sizeof(parse_out.digit_fields));
    parse_out.code_id = 0;

    is_digits_only = ((phone_hex[0] >> 4) & 0xF) == DIGIT_BIN_SPECIAL;
    if (is_digits_only)
        PG_RETURN_NULL();

    is_mask = phone_hex[hex_len - 1] == DIGIT_BIN_HEX_SPECIAL_SPECIAL;

    if (is_mask)
        hex_len = hex_len / 3; // Remove mask from digits hex len.

    {
        struct digit_letter digit_letter;
        int last_hex_index = hex_len - 1;
        int hex_index;
        digit_letter.letter_index = 0;
        digit_letter.invalid_char = 0;
        for(hex_index = 0; hex_index < hex_len; ++hex_index) {
            int hex_value = phone_hex[hex_index];
            int nibble1 = ((hex_value >> 4) & 0xF) + DIGIT_BIN_OFFSET;
            int nibble2 = (hex_value & 0xF) + DIGIT_BIN_OFFSET;

            digit_letter.value = nibble1;
            parse_digit(digit_letter, &parse_out);
            if (parse_out.parse_state == STATE_ERROR)
                break;
            if (parse_out.digit_fields[parse_out.digit_pos_next - 1] != FIELD_CALLING_CODE)
                PG_RETURN_INT16(calling_code_get(&parse_out));

            // Do not read the filler nibble.
            if (hex_index == last_hex_index && nibble2 == DIGIT_SPECIAL)
                break;

            digit_letter.value = nibble2;
            parse_digit(digit_letter, &parse_out);
            if (parse_out.parse_state == STATE_ERROR)
                break;
            if (parse_out.digit_fields[parse_out.digit_pos_next - 1] != FIELD_CALLING_CODE)
                PG_RETURN_INT16(calling_code_get(&parse_out));
        }

        if (parse_out.parse_state == STATE_ERROR)
            report_byte_format_error(&parse_out, hex_index);
    }

    ereport(
        ERROR,
        (   errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
            errmsg("Cannot find calling code.")
        )
    );
}


PG_FUNCTION_INFO_V1(telephone_service_get);
Datum telephone_service_get(PG_FUNCTION_ARGS) {
    bytea   *vlena      = PG_GETARG_BYTEA_P(0);
    char    *phone_hex  = VARDATA(vlena);
    int     hex_len     = VARSIZE(vlena) - VARHDRSZ;
    int     is_digits_only;
    int     is_mask;

    struct parse_buffer parse_out;
    parse_out.parse_state       = STATE_CALLING_CODE_START;  // Only valid if !is_digits_only
    parse_out.digit_pos_next    = 0;
    parse_out.error_code        = ERROR_PARSE_STATE_OR_OTHER;
    parse_out.service_type      = TSERV_NEED_TO_PARSE;
    //memset(parse_out.digit_values, 0, sizeof(parse_out.digit_values));
    //memset(parse_out.digit_valuesmask, 0, sizeof(parse_out.digit_valuesmask));
    //memset(parse_out.digit_fields, 0, sizeof(parse_out.digit_fields));
    parse_out.code_id = 0;

    is_digits_only = ((phone_hex[0] >> 4) & 0xF) == DIGIT_BIN_SPECIAL;
    if (is_digits_only)
        PG_RETURN_NULL();

    is_mask = phone_hex[hex_len - 1] == DIGIT_BIN_HEX_SPECIAL_SPECIAL;

    if (is_mask)
        hex_len = hex_len / 3; // Remove mask from digits hex len.

    init_oids_service();
    {
        struct digit_letter digit_letter;
        int last_hex_index = hex_len - 1;
        int hex_index;
        digit_letter.letter_index = 0;
        digit_letter.invalid_char = 0;
        for(hex_index = 0; hex_index < hex_len; ++hex_index) {
            int hex_value = phone_hex[hex_index];
            int nibble1 = ((hex_value >> 4) & 0xF) + DIGIT_BIN_OFFSET;
            int nibble2 = (hex_value & 0xF) + DIGIT_BIN_OFFSET;

            digit_letter.value = nibble1;
            parse_digit(digit_letter, &parse_out);
            if (parse_out.parse_state == STATE_ERROR)
                break;
            //if (parse_out.service_type != TSERV_NEED_TO_PARSE)
            //    PG_RETURN_OID(service_oids[service_get(&parse_out)]);

            // Do not read the filler nibble.
            if (hex_index == last_hex_index && nibble2 == DIGIT_SPECIAL)
                break;

            digit_letter.value = nibble2;
            parse_digit(digit_letter, &parse_out);
            if (parse_out.parse_state == STATE_ERROR)
                break;
            //if (parse_out.service_type != TSERV_NEED_TO_PARSE)
            //    PG_RETURN_OID(service_oids[service_get(&parse_out)]);
        }

        if (parse_out.parse_state == STATE_ERROR)
            report_byte_format_error(&parse_out, hex_index);
    }

    if (parse_out.service_type != TSERV_NEED_TO_PARSE)
        PG_RETURN_OID(service_oids[service_get(&parse_out)]);

    ereport(
        ERROR,
        (   errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
            errmsg("Cannot find service code.")
        )
    );
}


PG_FUNCTION_INFO_V1(telephone_fictitious_get);
Datum telephone_fictitious_get(PG_FUNCTION_ARGS) {
    bytea   *vlena          = PG_GETARG_BYTEA_P(0);
    char    *phone_hex      = VARDATA(vlena);
    int     hex_len         = VARSIZE(vlena) - VARHDRSZ;
    int     is_digits_only;
    int     is_mask;

    struct parse_buffer parse_out;
    parse_out.parse_state       = STATE_CALLING_CODE_START;  // Only valid if !is_digits_only
    parse_out.digit_pos_next    = 0;
    parse_out.error_code        = ERROR_PARSE_STATE_OR_OTHER;
    parse_out.service_type      = TSERV_NEED_TO_PARSE;
    //memset(parse_out.digit_values, 0, sizeof(parse_out.digit_values));
    //memset(parse_out.digit_valuesmask, 0, sizeof(parse_out.digit_valuesmask));
    //memset(parse_out.digit_fields, 0, sizeof(parse_out.digit_fields));
    parse_out.code_id = 0;

    is_digits_only = ((phone_hex[0] >> 4) & 0xF) == DIGIT_BIN_SPECIAL;
    if (is_digits_only)
        PG_RETURN_NULL();

    is_mask = phone_hex[hex_len - 1] == DIGIT_BIN_HEX_SPECIAL_SPECIAL;

    if (is_mask)
        hex_len = hex_len / 3; // Remove mask from digits hex len.

    {
        struct digit_letter digit_letter;
        int last_hex_index = hex_len - 1;
        int hex_index;
        digit_letter.letter_index = 0;
        digit_letter.invalid_char = 0;
        for(hex_index = 0; hex_index < hex_len; ++hex_index) {
            int hex_value = phone_hex[hex_index];
            int nibble1 = ((hex_value >> 4) & 0xF) + DIGIT_BIN_OFFSET;
            int nibble2 = (hex_value & 0xF) + DIGIT_BIN_OFFSET;

            digit_letter.value = nibble1;
            parse_digit(digit_letter, &parse_out);
            if (parse_out.parse_state == STATE_ERROR)
                break;

            // Do not read the filler nibble.
            if (hex_index == last_hex_index && nibble2 == DIGIT_SPECIAL)
                break;

            digit_letter.value = nibble2;
            parse_digit(digit_letter, &parse_out);
            if (parse_out.parse_state == STATE_ERROR)
                break;
        }

        if (parse_out.parse_state == STATE_ERROR)
            report_byte_format_error(&parse_out, hex_index);
    }

    init_oids_fictitious();

    switch (calling_code_get(&parse_out)) {
        case 1:
            if (parse_out.digit_pos_next < 10)
                PG_RETURN_NULL();

            if (
                parse_out.digit_values[1] == 8 &&
                parse_out.digit_values[2] == 0 &&
                parse_out.digit_values[3] == 0) {
                if (
                    parse_out.digit_values[4] == 5 &&
                    parse_out.digit_values[5] == 5 &&
                    parse_out.digit_values[6] == 5) {
                    if (parse_out.digit_values[ 7] == 0 &&
                        parse_out.digit_values[ 8] == 1 &&
                        parse_out.digit_values[ 9] == 9 &&
                        parse_out.digit_values[10] == 9) {
                        PG_RETURN_OID(fictitious_oids[TFICT_FICTITIOUS_RESERVED]);
                    } else {
                        PG_RETURN_OID(fictitious_oids[TFICT_REAL_MISTAKEN]);
                    }
                }
            } else if (
                parse_out.digit_values[1] == 8 &&
                parse_out.digit_values[2] >= 4 &&
                parse_out.digit_values[2] <= 8 &&
                parse_out.digit_values[3] == parse_out.digit_values[2]) {
                if (
                    parse_out.digit_values[4] == 5 &&
                    parse_out.digit_values[5] == 5 &&
                    parse_out.digit_values[6] == 5) {
                    PG_RETURN_OID(fictitious_oids[TFICT_FICTITIOUS_RESERVED]);
                }
            } else if (
                parse_out.digit_values[1] == 6 &&
                parse_out.digit_values[2] == 0 &&
                parse_out.digit_values[3] == 0) {
                if (
                    parse_out.digit_values[4] == 5 &&
                    parse_out.digit_values[5] == 5 &&
                    parse_out.digit_values[6] == 5) {
                    PG_RETURN_OID(fictitious_oids[TFICT_FICTITIOUS_RESERVED]);
                }
            } else if ( // All exchanges.
                parse_out.digit_values[4] == 5 &&
                parse_out.digit_values[5] == 5 &&
                parse_out.digit_values[6] == 5) {
                if (parse_out.digit_values[7] == 0 &&
                    parse_out.digit_values[8] == 1) {
                    PG_RETURN_OID(fictitious_oids[TFICT_FICTITIOUS_RESERVED]);
                } else {
                    PG_RETURN_OID(fictitious_oids[TFICT_REAL_MISTAKEN]);
                }
            } else if (
                parse_out.digit_values[ 1] == 2 && // Munich, Scott Pilgrim vs. the World, The Adjustment Bureau, and
                parse_out.digit_values[ 2] == 1 && // Definitely, Maybe.
                parse_out.digit_values[ 3] == 2 &&
                parse_out.digit_values[ 4] == 6 &&
                parse_out.digit_values[ 5] == 6 &&
                parse_out.digit_values[ 6] == 4 &&
                parse_out.digit_values[ 7] == 7 &&
                parse_out.digit_values[ 8] == 6 &&
                parse_out.digit_values[ 9] == 6 &&
                parse_out.digit_values[10] == 5) {
                    PG_RETURN_OID(fictitious_oids[TFICT_REAL_FICTITIOUS]);
            } else if (
                parse_out.digit_values[ 1] == 8 && // The Office
                parse_out.digit_values[ 2] == 0 &&
                parse_out.digit_values[ 3] == 0 &&
                parse_out.digit_values[ 4] == 9 &&
                parse_out.digit_values[ 5] == 8 &&
                parse_out.digit_values[ 6] == 4 &&
                parse_out.digit_values[ 7] == 3 &&
                parse_out.digit_values[ 8] == 6 &&
                parse_out.digit_values[ 9] == 7 &&
                parse_out.digit_values[10] == 2) {
                    PG_RETURN_OID(fictitious_oids[TFICT_REAL_FICTITIOUS]);
            }
            PG_RETURN_OID(fictitious_oids[TFICT_REAL]);
        break;
        case 44:
            if (parse_out.digit_pos_next < 10)
                PG_RETURN_NULL();

            if (
                parse_out.digit_values[2] == 1 &&
                parse_out.digit_values[3] == 6 &&
                parse_out.digit_values[4] == 3 &&
                parse_out.digit_values[5] == 2) {
                if (parse_out.digit_values[6] == 9 &&
                    parse_out.digit_values[7] == 6 &&
                    parse_out.digit_values[8] == 0) {
                    PG_RETURN_OID(fictitious_oids[TFICT_FICTITIOUS_RESERVED]);
                } else {
                    PG_RETURN_OID(fictitious_oids[TFICT_REAL_MISTAKEN]);
                }
            } else if (
                parse_out.digit_values[2] == 1 &&
                parse_out.digit_values[3] == 1 &&
                parse_out.digit_values[4] >= 3 && parse_out.digit_values[4] <= 8) {
                if (parse_out.digit_values[5] == 4 &&
                    parse_out.digit_values[6] == 9 &&
                    parse_out.digit_values[7] == 6 &&
                    parse_out.digit_values[8] == 0) {
                    PG_RETURN_OID(fictitious_oids[TFICT_FICTITIOUS_RESERVED]);
                }
            } else if (
                parse_out.digit_values[2] == 1 &&
                parse_out.digit_values[3] >= 2 && parse_out.digit_values[3] <= 6 &&
                parse_out.digit_values[4] == 1) {
                if (parse_out.digit_values[5] == 4 &&
                    parse_out.digit_values[6] == 9 &&
                    parse_out.digit_values[7] == 6 &&
                    parse_out.digit_values[8] == 0) {
                    PG_RETURN_OID(fictitious_oids[TFICT_FICTITIOUS_RESERVED]);
                }
            } else if (
                parse_out.digit_values[2] == 2 &&
                parse_out.digit_values[3] == 0 &&
                parse_out.digit_values[4] == 7 &&
                parse_out.digit_values[5] == 9 &&
                parse_out.digit_values[6] == 4 &&
                parse_out.digit_values[7] == 6 &&
                parse_out.digit_values[8] == 0) {
                PG_RETURN_OID(fictitious_oids[TFICT_FICTITIOUS_RESERVED]);
            } else if (
                parse_out.digit_values[2] == 1 &&
                parse_out.digit_values[3] == 9 &&
                parse_out.digit_values[4] == 1 &&
                parse_out.digit_values[5] == 4 &&
                parse_out.digit_values[6] == 9 &&
                parse_out.digit_values[7] == 8 &&
                parse_out.digit_values[8] == 0) {
                PG_RETURN_OID(fictitious_oids[TFICT_FICTITIOUS_RESERVED]);
            } else if (
                parse_out.digit_values[2] == 2 &&
                parse_out.digit_values[3] == 8 &&
                parse_out.digit_values[4] == 9 &&
                parse_out.digit_values[5] == 0 &&
                parse_out.digit_values[6] == 1 &&
                parse_out.digit_values[7] == 8 &&
                parse_out.digit_values[8] == 0) {
                PG_RETURN_OID(fictitious_oids[TFICT_FICTITIOUS_RESERVED]);
            } else if (
                parse_out.digit_values[2] == 2 &&
                parse_out.digit_values[3] == 9 &&
                parse_out.digit_values[4] == 2 &&
                parse_out.digit_values[5] == 0 &&
                parse_out.digit_values[6] == 1 &&
                parse_out.digit_values[7] == 8 &&
                parse_out.digit_values[8] == 0) {
                PG_RETURN_OID(fictitious_oids[TFICT_FICTITIOUS_RESERVED]);
            } else if (
                parse_out.digit_values[2] == 7 &&
                parse_out.digit_values[3] == 7 &&
                parse_out.digit_values[4] == 0 &&
                parse_out.digit_values[5] == 0 &&
                parse_out.digit_values[6] == 9 &&
                parse_out.digit_values[7] == 0 &&
                parse_out.digit_values[8] == 0) {
                PG_RETURN_OID(fictitious_oids[TFICT_FICTITIOUS_RESERVED]);
            } else if (
                parse_out.digit_values[2] == 8 &&
                parse_out.digit_values[3] == 0 &&
                parse_out.digit_values[4] == 8 &&
                parse_out.digit_values[5] == 1 &&
                parse_out.digit_values[6] == 5 &&
                parse_out.digit_values[7] == 7 &&
                parse_out.digit_values[8] == 0) {
                PG_RETURN_OID(fictitious_oids[TFICT_FICTITIOUS_RESERVED]);
            } else if (
                parse_out.digit_values[2] == 9 &&
                parse_out.digit_values[3] == 0 &&
                parse_out.digit_values[4] == 9 &&
                parse_out.digit_values[5] == 8 &&
                parse_out.digit_values[6] == 7 &&
                parse_out.digit_values[7] == 9 &&
                parse_out.digit_values[8] == 0) {
                PG_RETURN_OID(fictitious_oids[TFICT_FICTITIOUS_RESERVED]);
            } else if (
                parse_out.digit_values[2] == 3 &&
                parse_out.digit_values[3] == 0 &&
                parse_out.digit_values[4] == 6 &&
                parse_out.digit_values[5] == 9 &&
                parse_out.digit_values[6] == 9 &&
                parse_out.digit_values[7] == 9 &&
                parse_out.digit_values[8] == 0) {
                PG_RETURN_OID(fictitious_oids[TFICT_FICTITIOUS_RESERVED]);
            }
            PG_RETURN_OID(fictitious_oids[TFICT_REAL]);
        break;
    }

    PG_RETURN_NULL();
}

static int get_digit_end_before_field(struct parse_buffer *parse_in, uint digit_field) {
    int digit_index;
    for(digit_index = 0; digit_index < parse_in->digit_pos_next; ++digit_index) {
        if (parse_in->digit_fields[digit_index] == digit_field) {
            return digit_index;
        }
    }
    return digit_index;
}

static bytea * telephone_bytea_trim(bytea *value, int len) {
    int     new_char_len;
    char    *old_chara          = VARDATA(value);
    char    *new_chara;
    int     remove_last_nibble  = 0;
    bytea *result;

    if (mode_get(value) == TMODE_CALLING_CODE) {
        new_char_len = len * .5;
        if ((len & 1) == 1) {
            remove_last_nibble = 1;
            new_char_len++;
        }
    } else {
        new_char_len = (len + 1) * .5;
        if ((len & 1) == 0) {
            remove_last_nibble = 1;
            new_char_len++;
        }
    }

    result = (bytea *) palloc(new_char_len + VARHDRSZ);
    SET_VARSIZE(result, new_char_len + VARHDRSZ);
    new_chara = VARDATA(result);
    memcpy(new_chara, old_chara, new_char_len);

    if (remove_last_nibble) {
        new_chara[new_char_len - 1] = new_chara[new_char_len - 1] & 240;
    }

    return result;
}

static int get_number_of_digits(uint number) {
    if (number < 10) return 1;
    if (number < 100) return 2;
    if (number < 1000) return 3;
    if (number < 10000) return 4;
    if (number < 100000) return 5;
    if (number < 1000000) return 6;
    if (number < 10000000) return 7;
    if (number < 100000000) return 8;
    if (number < 1000000000) return 9;
    return 10;
}

/*static int find_first_non_number(struct parse_buffer *parse_in) {
    uint digit_pos;
    for (digit_pos = 0; digit_pos < parse_in->digit_pos_next; digit_pos++) {
        if (parse_in->digit_values[digit_pos] < 0 || parse_in->digit_values[digit_pos] > 9)
            return digit_pos;
        if ((parse_in->digit_valuesmask[digit_pos] & VALUEMASK_CONFIRM) == VALUEMASK_CONFIRM)
            return digit_pos + 1;
        if (((parse_in->digit_valuesmask[digit_pos] >> VALUEBIT_PAUSE) & VALUEPARTMASK_PAUSE) > 0)
            return digit_pos + 1;
    }
    return -1;
}

PG_FUNCTION_INFO_V1(telephone_number_only_part);
Datum
telephone_number_only_part(PG_FUNCTION_ARGS) {
    bytea       *vlena              = PG_GETARG_BYTEA_P(0);
    struct parse_buffer parse_in    = telephone_bytea_to_parse_buffer(vlena);
    int         digit_pos           = find_first_non_number(&parse_in);

    if (digit_pos == -1)
        PG_RETURN_BYTEA_P(vlena);

    PG_RETURN_BYTEA_P(telephone_bytea_trim(vlena, digit_pos));
}*/

PG_FUNCTION_INFO_V1(telephone_geo_parts_get);
Datum telephone_geo_parts_get(PG_FUNCTION_ARGS) {
    bytea       *vlena              = PG_GETARG_BYTEA_P(0);
    bool        include_calling_code= PG_GETARG_BOOL(1);
    bool        include_subscriber  = PG_GETARG_BOOL(2);
    int         parts_count         = 0;
    int         calling_code        = 0;
    int         run_geo             = 1;
    Datum       *part_elems;
    ArrayType   *result;
    struct parse_buffer parse_in    = telephone_bytea_to_parse_buffer(vlena);
    int         part_lens[MAX_DIGITS];

    if (mode_get(vlena) == TMODE_CALLING_CODE) {
        calling_code = calling_code_get(&parse_in);

        if (include_calling_code)
            part_lens[parts_count++] = get_number_of_digits(calling_code);

        run_geo = geo_is(&parse_in);
    }

    if (run_geo) {
        switch (calling_code) {
            case 1:
                part_lens[parts_count++] = 4;
                part_lens[parts_count++] = 7;
                break;
            case 33:
                part_lens[parts_count++] = get_digit_end_before_field(&parse_in, FIELD_FRANCE_SUB);
                break;
            case 39:
                part_lens[parts_count++] = get_digit_end_before_field(&parse_in, FIELD_ITALY_SUB1);
                break;
            case 44:
                part_lens[parts_count++] = get_digit_end_before_field(&parse_in, FIELD_UK_SUB1);
                break;
            case 61:
                part_lens[parts_count++] = 3; // FIELD_AUSTRALIA_SUB1
                break;
            case 7:
                part_lens[parts_count++] = 4;
                break;
            case 81:
                {
                    int get_sub_1 = get_digit_end_before_field(&parse_in, FIELD_JAPAN_SUB1);
                    int get_sub_2 = get_digit_end_before_field(&parse_in, FIELD_JAPAN_SUB2);
                    if (get_sub_1 > get_sub_2)
                        get_sub_1 = get_sub_2; // Larger areas begin with SUB2.

                    part_lens[parts_count++] = get_sub_1;
                }
                break;
            case 91:
                part_lens[parts_count++] = get_digit_end_before_field(&parse_in, FIELD_INDIA_SUB1);
                break;
            default:
                {
                    int digit_pos;
                    int digit_start = 1;
                    int digit_end = parse_in.digit_pos_next;

                    if (include_calling_code)
                        digit_start = 0;

                    if (!include_subscriber)
                        digit_end--;

                    for (digit_pos = digit_start; digit_pos < digit_end; digit_pos++ ) {
                        if (parse_in.digit_values[digit_pos] < 0 || parse_in.digit_values[digit_pos] > 9)
                            break;
                        part_lens[parts_count++] = digit_pos + 1;
                        if ((parse_in.digit_valuesmask[digit_pos] & VALUEMASK_CONFIRM) == VALUEMASK_CONFIRM)
                            break;
                        if (((parse_in.digit_valuesmask[digit_pos] >> VALUEBIT_PAUSE) & VALUEPARTMASK_PAUSE) > 0)
                            break;
                    }
                }
        }
    }

    if (calling_code != 0 && include_subscriber)
        part_lens[parts_count++] = get_digit_end_before_field(&parse_in, FIELD_EXTENSION);

    if (parse_in.digit_pos_next == 0) {
        parts_count = 0;
    }

    part_elems       = (Datum*)palloc(parts_count * sizeof(Datum));
    {
        int part_index = 0;
        for (; part_index < parts_count; part_index++ ) {
            part_elems[part_index] = (Datum)telephone_bytea_trim(vlena, part_lens[part_index]);
            //telephone_parse_buffer_to_bytea(&parse_in, 0, part_lens[part_index], 0);
        }
    }

    //init_typlen_text();
    init_typlen_telephone();
    //result = construct_array(part_elems, parts_count, TEXTOID, typlen_text, typlen_text_typbyval, typlen_text_typalign);
    result = construct_array(part_elems, parts_count, typlen_telephone_oid, typlen_telephone, typlen_telephone_typbyval,
                             typlen_telephone_typalign);

    PG_RETURN_ARRAYTYPE_P(result);
}

PG_FUNCTION_INFO_V1(telephone_ident_bytes_get);
Datum telephone_ident_bytes_get(PG_FUNCTION_ARGS) {
    bytea   *vlena          = PG_GETARG_BYTEA_P(0);
    char    *phone_hex      = VARDATA(vlena);
    int     in_hex_len      = VARSIZE(vlena) - VARHDRSZ;
    int     is_mask;
    char    out_hex[MAX_DIGITS];
    int     out_hex_index   = 0;
    char   *bytea_data;
    bytea  *result;

    is_mask = phone_hex[in_hex_len - 1] == DIGIT_BIN_HEX_SPECIAL_SPECIAL;

    if (is_mask)
        in_hex_len = in_hex_len / 3; // Remove mask from digits hex len.

    {
        int last_in_hex_index = in_hex_len - 1;
        int in_hex_index;
        char out_nibbles;
        int out_on_nibble2 = 0;
        for(in_hex_index = 0; in_hex_index < in_hex_len; ++in_hex_index) {
            int in_hex_value = phone_hex[in_hex_index];
            int in_nibble_1 = ((in_hex_value >> 4) & 0xF) + DIGIT_BIN_OFFSET;
            int in_nibble_2 = (in_hex_value & 0xF) + DIGIT_BIN_OFFSET;

            // Do not read the digits qualifier
            if (in_nibble_1 >= 0) {
                if (out_on_nibble2) {
                    out_nibbles = out_nibbles | in_nibble_1;
                    out_hex[out_hex_index++] = out_nibbles;
                    out_on_nibble2 = 0;
                } else {
                    out_nibbles = in_nibble_1 << 4;
                    out_on_nibble2 = 1;
                }
            }

            // Do not read the filler nibble.
            if (in_hex_index == last_in_hex_index && in_nibble_2 == DIGIT_SPECIAL)
                break;

            if (in_nibble_2 >= 0) {
                if (out_on_nibble2) {
                    out_nibbles = out_nibbles | in_nibble_2;
                    out_hex[out_hex_index++] = out_nibbles;
                    out_on_nibble2 = 0;
                } else {
                    out_nibbles = in_nibble_2 << 4;
                    out_on_nibble2 = 1;
                }
            }
        }

        if (out_on_nibble2)
            out_hex[out_hex_index++] = out_nibbles | 15;
    }

    result = palloc(out_hex_index + VARHDRSZ);
    bytea_data = VARDATA(result);
    memcpy(bytea_data, out_hex, out_hex_index);
    SET_VARSIZE(result, out_hex_index + VARHDRSZ);
    PG_RETURN_BYTEA_P(result);
}
