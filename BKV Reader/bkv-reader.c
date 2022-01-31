/*
 * bkv-reader.c
 * This file is part of BKV Reader
 *
 * Copyright (C) 2020 - Félix Arreola Rodríguez
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

typedef struct {
	uint16_t pos;
	char *word;
} DictWord;

typedef struct _Table Table;

typedef struct {
	uint32_t name_pos;
	char *name;

	uint8_t type;

	union {
		uint8_t boolean;
		uint8_t byte;
		float flotante;
		uint16_t short_int;
		uint32_t integer;
		char *string;

		Table *table;
	} value;
} TableEntry;

struct _Table {
	uint16_t pos;
	int n_entries;
	TableEntry *entries;
};

typedef struct {
	float translation[3];

	float rotation[4];

	float scale;
} Transform;

typedef struct {
	int n_words;
	DictWord *words;

	int n_tables;
	Table *tables;

	Table *root_table;

	int n_transforms;
	Transform *transforms;
} BKVDesc;

typedef struct {
	int id;
	char *name;
	int vertex_data_id;
	int renderable;
	int material;
	int back_face_culling;
	int max_influences;
} MeshData;

enum {
	ENCODING_NONE = 0,
	ENCODING_BYTE = 1,
	ENCODING_BYTE_SIGNED = 2,
	ENCODING_SHORT = 3,
	ENCODING_SHORT_SIGNED = 4,
	UNENCODED_BYTE = 5,
	UNENCODED_BYTE_SIGNED = 6,
	UNENCODED_SHORT = 7,
	UNENCODED_SHORT_SIGNED = 8
};

int do_endian = 0;

uint32_t endian_32 (uint32_t num) {
	unsigned char a[4];
	unsigned char b[4];
	if (!do_endian) return num;

	memcpy (a, &num, sizeof (a));

	b[0] = a[3];
	b[1] = a[2];
	b[2] = a[1];
	b[3] = a[0];

	memcpy (&num, b, sizeof (a));

	return num;
}

uint16_t endian_16 (uint16_t num) {
	unsigned char a[2];
	unsigned char b[2];
	if (!do_endian) return num;

	memcpy (a, &num, sizeof (a));

	b[0] = a[1];
	b[1] = a[0];

	memcpy (&num, b, sizeof (b));

	return num;
}

char *bkv_get_word (BKVDesc *bkv_desc, int pos) {
	int g;

	for (g = 0; g < bkv_desc->n_words; g++) {
		if (bkv_desc->words[g].pos == pos) {
			return bkv_desc->words[g].word;
		}
	}

	return NULL;
}

Table *bkv_get_table (BKVDesc *bkv_desc, int pos) {
	int g;

	for (g = 0; g < bkv_desc->n_tables; g++) {
		if (bkv_desc->tables[g].pos == pos) {
			return &bkv_desc->tables[g];
		}
	}

	return NULL;
}

#define TRY_READ_OR_GOTO(fd, buffer, bytes, location) \
	do { \
	if (read (fd, buffer, bytes) < bytes) { \
		printf ("Could not read %i bytes from file\n", bytes); \
		goto location; \
	} \
	} while (0)

int read_bkv (BKVDesc *bkv_desc, char *folder, char *filename) {
	uint32_t t32, *p32;
	uint16_t t16, *p16;
	uint8_t t8, *p8;
	float *pf;
	int fd_desc;

	int g, h;

	int bytes_strings;
	unsigned char *strings;
	int bytes_arrays;
	unsigned char *arrays;
	int bytes_tables;
	unsigned char *tables;

	unsigned char buffer[8192];

	int *string_places;

	int values_count;
	int tables_count;

	Table *current_table;

	snprintf (buffer, sizeof (buffer), "%s/%s", folder, filename);

	fd_desc = open (buffer, O_RDONLY);

	if (fd_desc < 0) {
		return -1;
	}

	TRY_READ_OR_GOTO (fd_desc, &t32, 4, error_desc);

	p8 = (uint8_t *) &t32;

	if (p8[3] == '$' && p8[2] == 'B' && p8[1] == 'K' && p8[0] == 'V') {
		do_endian = 1;
	} else if (p8[0] == '$' && p8[1] == 'B' && p8[2] == 'K' && p8[3] == 'V') {
		do_endian = 0;
	} else {
		printf ("No good $BKV/VKB$ signature header\n");
		goto error_desc;
	}

	TRY_READ_OR_GOTO (fd_desc, &t8, 1, error_desc);
	if (t8 != 0) {
		printf ("Version error\n");

		goto error_desc;
	}

	/* Omitir un byte */
	TRY_READ_OR_GOTO (fd_desc, &t8, 1, error_desc);

	/* Leer la cantidad de bytes en las cadenas */
	TRY_READ_OR_GOTO (fd_desc, &t32, 4, error_desc);

	bytes_strings = t32;
	strings = (unsigned char *) malloc (bytes_strings);
	TRY_READ_OR_GOTO (fd_desc, strings, bytes_strings, error_desc);

	/* Leer la cantidad de bytes en los arreglos */
	TRY_READ_OR_GOTO (fd_desc, &t32, 4, error_desc);

	bytes_arrays = t32;
	arrays = (unsigned char *) malloc (bytes_arrays);
	TRY_READ_OR_GOTO (fd_desc, arrays, bytes_arrays, error_desc);

	/* Leer la cantidad de bytes de las tablas */
	TRY_READ_OR_GOTO (fd_desc, &t32, 4, error_desc);

	bytes_tables = t32;
	tables = (unsigned char *) malloc (bytes_tables);
	TRY_READ_OR_GOTO (fd_desc, tables, bytes_tables, error_desc);

	close (fd_desc);

	string_places = (int *) malloc (sizeof (int) * bytes_strings);
	memset (string_places, 0, sizeof (int) * bytes_strings);

	/* Empezar a contar primero la cantidad de cadenas, y buscar en las tablas por referencias "secundarias" */
	for (h = 0, g = 0; g < bytes_strings; g++) {
		if (strings[g] == 0) {
			string_places[h] = 1;
			h = g + 1;
		}
	}

	/* Recorrer la tabla para extraer las cadenas "secundarias" */
	g = 0;
	tables_count = 0;
	while (g < bytes_tables) {
		p16 = (uint16_t *) &tables[g];
		g += 2;
		values_count = *p16;
		tables_count++;

		for (h = 0; h < values_count; h++) {
			p16 = (uint16_t *) &tables[g];
			g += 2;

			if ((*p16 & 0x8000) == 0) {
				string_places[*p16] = 1;
			}

			p8 = (uint8_t *) &tables[g];
			g++;

			switch (*p8) {
				case 2: /* TYPE_FLOAT */
				case 5: /* Type INT */
					g += 4;
					break;
				case 3: /* Type Byte */
					g++;
					break;
				case 4: /* Type Short */
				case 7: /* Type table */
					g += 2;
					break;
				case 6: /* Type String */
					p16 = (uint16_t *) &tables[g];
					g += 2;

					string_places[*p16] = 1;
					break;
				case 8:
				case 9:
					printf ("Error.\n");
					break;
			}
		}
	}

	/* Ahora sí, crear todas las cadenas en el arreglo */
	bkv_desc->n_words = 0;
	for (g = 0; g < bytes_strings; g++) {
		if (string_places[g] == 1) {
			bkv_desc->n_words++;
		}
	}

	bkv_desc->words = (DictWord *) malloc (sizeof (DictWord) * bkv_desc->n_words);
	h = 0;
	for (g = 0; g < bytes_strings; g++) {
		if (string_places[g] == 1) {
			bkv_desc->words[h].pos = g;
			bkv_desc->words[h].word = (char *) strdup (&strings[g]);
			h++;
		}
	}

	free (string_places);

	/* Siguiente paso, leer las tablas */
	bkv_desc->n_tables = tables_count;
	bkv_desc->tables = (Table *) malloc (sizeof (Table) * tables_count);

	/* bkv_get_word */
	g = 0;
	tables_count = 0;
	while (g < bytes_tables) {
		current_table = &bkv_desc->tables[tables_count];

		current_table->pos = g; /* TODO: Sumar los bytes de los strings + array */
		p16 = (uint16_t *) &tables[g];
		g += 2;

		current_table->n_entries = *p16;
		current_table->entries = (TableEntry *) malloc (sizeof (TableEntry) * current_table->n_entries);

		for (h = 0; h < current_table->n_entries; h++) {
			p16 = (uint16_t *) &tables[g];
			g += 2;

			current_table->entries[h].name_pos = *p16;
			current_table->entries[h].name = bkv_get_word (bkv_desc, *p16);

			p8 = (uint8_t *) &tables[g];
			g++;

			current_table->entries[h].type = *p8;

			switch (*p8) {
				case 0: /* FALSE */
				case 1: /* TRUE */
					current_table->entries[h].value.boolean = *p8;
					break;
				case 2: /* TYPE_FLOAT */
					pf = (float *) &tables[g];
					g += 4;
					current_table->entries[h].value.flotante = *pf;
					break;
				case 5: /* Type INT */
					p32 = (uint32_t *) &tables[g];
					g += 4;
					current_table->entries[h].value.integer = *p32;
					break;
				case 3: /* Type Byte */
					p8 = (uint8_t *) &tables[g];
					g++;
					current_table->entries[h].value.byte = *p8;
					break;
				case 4: /* Type Short */
					p16 = (uint16_t *) &tables[g];
					g += 2;
					current_table->entries[h].value.short_int = *p16;
					break;
				case 7: /* Type table */
					p16 = (uint16_t *) &tables[g];
					g += 2;

					current_table->entries[h].value.short_int = *p16;
					break;
				case 6: /* Type String */
					p16 = (uint16_t *) &tables[g];
					g += 2;

					current_table->entries[h].value.string = bkv_get_word (bkv_desc, *p16);
					break;
			}
		}

		tables_count++;
	}

	/* Ejecutar una segunda pasada a las tablas para resolver las referencias a otras tablas */
	for (g = 0; g < bkv_desc->n_tables; g++) {
		current_table = &bkv_desc->tables[g];

		for (h = 0; h < current_table->n_entries; h++) {
			if (current_table->entries[h].type == 7 /* Tipo Tabla */) {
				current_table->entries[h].value.table = bkv_get_table (bkv_desc, current_table->entries[h].value.short_int);
			}
		}
	}

	free (strings);
	free (arrays);
	free (tables);

	bkv_desc->root_table = &bkv_desc->root_table[0];

	return 0;
error_desc:
	close (fd_desc);

	return -1;
}

void print_table (Table *table, char *tab) {
	char buffer_tab[512];
	int g;
	TableEntry *entry;

	snprintf (buffer_tab, sizeof (buffer_tab), "%s\t", tab);

	for (g = 0; g < table->n_entries; g++) {
		entry = (TableEntry *) &table->entries[g];

		if (entry->name_pos & 0x8000) {
			/* Es un arreglo */
			printf ("%s[%i] => ", tab, entry->name_pos - 0x8000);
		} else {
			printf ("%s'%s' => ", tab, entry->name);
		}

		switch (entry->type) {
			case 0:
				printf ("FALSE,\n");
				break;
			case 1:
				printf ("TRUE,\n");
				break;
			case 2:
				printf ("%.2f,\n", entry->value.flotante);
				break;
			case 3:
				printf ("%i,\n", entry->value.byte);
				break;
			case 4:
				printf ("%i,\n", entry->value.short_int);
				break;
			case 5:
				printf ("%i,\n", entry->value.integer);
				break;
			case 6:
				printf ("\"%s\",\n", entry->value.string);
				break;
			case 7:
				printf ("{\n");
				print_table (entry->value.table, buffer_tab);
				printf ("%s},\n", tab);
				break;
			case 8:
			case 9:
				printf ("UNKNOWN,\n");
				break;
		}
	}
}

void read_transform (BKVDesc *bkv_desc, char *folder) {
	unsigned char buffer[8192];
	int fd_trans;
	uint8_t t8, byte_loc2, *p8;
	uint16_t t16, cant, *p16;
	uint32_t t32, *p32;
	int g;
	uint8_t element_size;
	int8_t *s8;
	int16_t *s16;

	float *pf;
	off_t pos;

	float floats[4];
	Transform *current_t;

	snprintf (buffer, sizeof (buffer), "%s/transform", folder);

	fd_trans = open (buffer, O_RDONLY);

	if (fd_trans < 0) {
		return;
	}

	TRY_READ_OR_GOTO (fd_trans, &t8, 1, error_trans);
	byte_loc2 = t8;

	if (byte_loc2 == ENCODING_BYTE) {
		element_size = 1;
		/* Unsigned byte / 255 */
	} else if (byte_loc2 == ENCODING_BYTE_SIGNED) {
		/* Leer un Byte */
		element_size = 1;
		/* SIGNED Byte / 127 */
	} else if (byte_loc2 == ENCODING_SHORT) {
		element_size = 2;
		/* unsigned short / 65535 */
	} else if (byte_loc2 == ENCODING_SHORT_SIGNED) {
		element_size = 2;
		/* SIGNED Short / 32767 */
	} else {
		printf ("---> Unhandled Transform type: %i\n", byte_loc2);
	}

	TRY_READ_OR_GOTO (fd_trans, &t16, 2, error_trans);
	cant = endian_16 (t16);

	printf ("Cant of transform pool: %i\n", cant);
	bkv_desc->n_transforms = cant;
	bkv_desc->transforms = (Transform *) malloc (sizeof (Transform) * cant);

	for (g = 0; g < cant; g++) {
		/* Leer:
		 * 3 flotantes
		 * 4 bloques del tipo byte_loc2
		 * 1 flotante
		 */
		current_t = &bkv_desc->transforms[g];

		printf ("Transformation [%i] =\n", g);

		TRY_READ_OR_GOTO (fd_trans, buffer, (3 * sizeof (float)), error_trans);
		/* Voltear el endianess */
		p32 = (uint32_t *) buffer;
		p32[0] = endian_32 (p32[0]);
		p32[1] = endian_32 (p32[1]);
		p32[2] = endian_32 (p32[2]);
		pf = (float *) buffer;

		printf ("\tTranslation: %.2f, %.2f, %.2f\n", pf[0], pf[1], pf[2]);
		current_t->translation[0] = pf[0];
		current_t->translation[1] = pf[1];
		current_t->translation[2] = pf[2];

		TRY_READ_OR_GOTO (fd_trans, buffer, 4 * element_size, error_trans);
		if (byte_loc2 == 1) {
			p8 = (int8_t *) buffer;
			floats[0] = ((float) p8[0]) / 255.0;
			floats[1] = ((float) p8[1]) / 255.0;
			floats[2] = ((float) p8[2]) / 255.0;
			floats[3] = ((float) p8[3]) / 255.0;
		} else if (byte_loc2 == 2) {
			s8 = (int8_t *) buffer;
			floats[0] = ((float) s8[0]) / 127.0;
			floats[1] = ((float) s8[1]) / 127.0;
			floats[2] = ((float) s8[2]) / 127.0;
			floats[3] = ((float) s8[3]) / 127.0;
		} else if (byte_loc2 == 3) {
			p16 = (uint16_t *) buffer;

			floats[0] = ((float) p16[0]) / 65535.0;
			floats[1] = ((float) p16[1]) / 65535.0;
			floats[2] = ((float) p16[2]) / 65535.0;
			floats[3] = ((float) p16[3]) / 65535.0;
		} else if (byte_loc2 == 4) {
			/* Primero voltear los bytes por el endianess */
			p16 = (uint16_t *) buffer;
			p16[0] = endian_16 (p16[0]);
			p16[1] = endian_16 (p16[1]);
			p16[2] = endian_16 (p16[2]);
			p16[3] = endian_16 (p16[3]);

			s16 = (int16_t *) buffer;
			floats[0] = ((float) s16[0]) / 32767.0;
			floats[1] = ((float) s16[1]) / 32767.0;
			floats[2] = ((float) s16[2]) / 32767.0;
			floats[3] = ((float) s16[3]) / 32767.0;
		}
		printf ("\tRotation: %.4f, %.4f, %.4f, %.4f\n", floats[0], floats[1], floats[2], floats[3]);
		current_t->rotation[0] = pf[0];
		current_t->rotation[1] = pf[1];
		current_t->rotation[2] = pf[2];
		current_t->rotation[3] = pf[3];

		TRY_READ_OR_GOTO (fd_trans, buffer, sizeof (float), error_trans);

		p32 = (uint32_t *) buffer;
		p32[0] = endian_32 (p32[0]);
		pf = (float *) buffer;
		printf ("\tScale: %.2f\n", pf[0]);
		current_t->scale = pf[0];
	}

	close (fd_trans);

	return;
error_trans:
	printf ("Skipping....\n");
	close (fd_trans);
}

void read_skeleton (BKVDesc *bkv_desc, char *folder) {
	int fd_skel;
	unsigned char buffer[8192];
	uint8_t u8, *p8;
	uint16_t u16, *p16;
	uint32_t u32, *p32;
	int bones;
	int g, h;
	char name[512];

	snprintf (buffer, sizeof (buffer), "%s/skeleton", folder);

	fd_skel = open (buffer, O_RDONLY);

	if (fd_skel < 0) {
		return;
	}

	TRY_READ_OR_GOTO (fd_skel, &u8, 1, error_skeleton);
	bones = u8;

	for (g = 0; g < bones; g++) {
		TRY_READ_OR_GOTO (fd_skel, &u16, 2, error_skeleton);
		u16 = endian_16 (u16);
		TRY_READ_OR_GOTO (fd_skel, name, u16, error_skeleton);
		name[u16] = 0;
		printf ("Skeleton[%i]: %s\n", g, name);

		TRY_READ_OR_GOTO (fd_skel, &u8, 1, error_skeleton);
		printf (" -> Parent: %i\n", u8);

		TRY_READ_OR_GOTO (fd_skel, &u8, 1, error_skeleton);
		printf ("Childs count: %i\n", u8);
		uint8_t childs = u8;

		if (childs > 0) {
			for (h = 0; h < childs; h++) {
				TRY_READ_OR_GOTO (fd_skel, &u8, 1, error_skeleton);
				printf ("\tChild: %i\n", u8);
			}
		}

		TRY_READ_OR_GOTO (fd_skel, &u16, 2, error_skeleton);
		u16 = endian_16 (u16);
		printf ("Use tranform: %i\n", u16);

		TRY_READ_OR_GOTO (fd_skel, &u16, 2, error_skeleton);
		u16 = endian_16 (u16);
		printf ("Use INV tranform: %i\n", u16);
	}

	close (fd_skel);
	return;
error_skeleton:
	printf ("Skipping....\n");
	close (fd_skel);
}

Table *get_key_as_table (Table *table, char *key) {
	int g;
	TableEntry *entry;

	for (g = 0; g < table->n_entries; g++) {
		entry = (TableEntry *) &table->entries[g];

		if (entry->name_pos & 0x8000) {
			/* Es un arreglo */
			//printf ("%s[%i] => ", tab, entry->name_pos - 0x8000);
		} else {
			if (strcmp (key, entry->name) == 0) {
				return entry->value.table;
			}
		}
	}

	return NULL;
}

uint32_t get_key_as_int (Table *table, char *key) {
	int g;
	TableEntry *entry;

	for (g = 0; g < table->n_entries; g++) {
		entry = (TableEntry *) &table->entries[g];

		if (entry->name_pos & 0x8000) {
			/* Es un arreglo */
			//printf ("%s[%i] => ", tab, entry->name_pos - 0x8000);
		} else {
			if (strcmp (key, entry->name) == 0) {
				return entry->value.integer;
			}
		}
	}

	return 0;
}

uint8_t get_key_as_boolean (Table *table, char *key) {
	int g;
	TableEntry *entry;

	for (g = 0; g < table->n_entries; g++) {
		entry = (TableEntry *) &table->entries[g];

		if (entry->name_pos & 0x8000) {
			/* Es un arreglo */
			//printf ("%s[%i] => ", tab, entry->name_pos - 0x8000);
		} else {
			if (strcmp (key, entry->name) == 0) {
				return entry->value.boolean;
			}
		}
	}

	return 0;
}

char *get_key_as_string (Table *table, char *key) {
	int g;
	TableEntry *entry;

	for (g = 0; g < table->n_entries; g++) {
		entry = (TableEntry *) &table->entries[g];

		if (entry->name_pos & 0x8000) {
			/* Es un arreglo */
			//printf ("%s[%i] => ", tab, entry->name_pos - 0x8000);
		} else {
			if (strcmp (key, entry->name) == 0) {
				return entry->value.string;
			}
		}
	}

	return NULL;
}

Table *get_index_as_table (Table *table, int pos) {
	int g;
	TableEntry *entry;

	for (g = 0; g < table->n_entries; g++) {
		entry = (TableEntry *) &table->entries[g];

		if (entry->name_pos & 0x8000) {
			/* Es un arreglo */
			if (entry->name_pos - 0x8000 == pos) {
				return entry->value.table;
			}
		}
	}

	return NULL;
}

int get_num_values (Table *table) {
	int g;
	TableEntry *entry;
	int c;

	c = 0;
	for (g = 0; g < table->n_entries; g++) {
		entry = (TableEntry *) &table->entries[g];

		if (entry->name_pos & 0x8000) {
			/* Es un arreglo */
			c++;
		} else {
			printf ("Warning: Valor no arreglo en la tabla\n");
		}
	}

	return c;
}

void read_indices (char *folder, char *filename) {
	unsigned char buffer[8192];
	int fd_index;
	uint8_t u8, loc_4, loc_7;
	int8_t s8;
	uint16_t u16;
	uint32_t u32, loc_5, loc_11, loc_12;
	int g, h;

	snprintf (buffer, sizeof (buffer), "%s/%s", folder, filename);

	fd_index = open (buffer, O_RDONLY);

	if (fd_index < 0) {
		return;
	}

	TRY_READ_OR_GOTO (fd_index, &u8, 1, error_index);

	loc_4 = u8;

	if (loc_4 == 1) {
		TRY_READ_OR_GOTO (fd_index, &u32, 4, error_index);
		u32 = endian_32 (u32);
	} else {
		TRY_READ_OR_GOTO (fd_index, &u16, 2, error_index);
		u16 = endian_16 (u16);
		u32 = u16;
	}

	loc_5 = u32;

	/* ¿Arreglo de loc_5 * 2? */
	TRY_READ_OR_GOTO (fd_index, &s8, 1, error_index);

	loc_7 = 0;
	if (s8 > 0) {
		loc_7 = 1;
	}

	if (loc_7 == 0) {
		printf ("ESTA CONDICION FALSA\n");
		for (g = 0; g < loc_5; g++) {
			if (loc_4 == 1) {
				TRY_READ_OR_GOTO (fd_index, &u32, 4, error_index);
				u32 = endian_32 (u32);
			} else {
				TRY_READ_OR_GOTO (fd_index, &u16, 2, error_index);
				u16 = endian_16 (u16);
				u32 = u16;
			}
			printf ("Valores de este arreglo: %i\n", u32);
		}

		return;
	}

	printf ("Valores de este arreglo: %i\n", loc_5);
	for (g = 0; g < loc_5;) {
		TRY_READ_OR_GOTO (fd_index, &u8, 1, error_index);

		if (u8 == 0) {
			/* El primer entero indica cuántos valos a leer */
			if (loc_4 == 1) {
				TRY_READ_OR_GOTO (fd_index, &u32, 4, error_index);
				loc_12 = endian_32 (u32);
			} else {
				TRY_READ_OR_GOTO (fd_index, &u16, 2, error_index);
				u16 = endian_16 (u16);
				loc_12 = u16;
			}
			for (h = 0; h < loc_12; h++) {
				if (loc_4 == 1) {
					TRY_READ_OR_GOTO (fd_index, &u32, 4, error_index);
					u32 = endian_32 (u32);
				} else {
					TRY_READ_OR_GOTO (fd_index, &u16, 2, error_index);
					u16 = endian_16 (u16);
					u32 = u16;
				}
				printf ("Short value: %i\n", u32);
			}
		} else {
			/* Run length encoded, valor + cantidad de valores consecutivos */
			/* Leer la local 11 */
			if (loc_4 == 1) {
				TRY_READ_OR_GOTO (fd_index, &u32, 4, error_index);
				loc_11 = endian_32 (u32);
			} else {
				TRY_READ_OR_GOTO (fd_index, &u16, 2, error_index);
				u16 = endian_16 (u16);
				loc_11 = u16;
			}

			/* Leer la local 12 */
			if (loc_4 == 1) {
				TRY_READ_OR_GOTO (fd_index, &u32, 4, error_index);
				loc_12 = endian_32 (u32);
			} else {
				TRY_READ_OR_GOTO (fd_index, &u16, 2, error_index);
				u16 = endian_16 (u16);
				loc_12 = u16;
			}

			for (h = 0; h < loc_12; h++) {
				printf ("Short value: %i\n", loc_11 + h);
			}
		}

		g = g + loc_12;
	}

	close (fd_index);
	return;
error_index:
	printf ("Skipping read index %s....\n", filename);
	close (fd_index);
}

void load_mesh_data (MeshData *mesh, Table *table, char *folder) {
	char name[128];
	mesh->id = get_key_as_int (table, "id");
	mesh->name = get_key_as_string (table, "name");
	mesh->vertex_data_id = get_key_as_int (table, "vert");
	mesh->renderable = get_key_as_boolean (table, "nonrendered");

	if (mesh->renderable == 0) {
		mesh->renderable = 1;
	} else {
		mesh->renderable = 0;
	}

	if (mesh->renderable) {
		/* ¿Qué hago aquí? */
		mesh->material = get_key_as_int (table, "material");
		mesh->back_face_culling = get_key_as_boolean (table, "bfculling");
		mesh->max_influences = get_key_as_int (table, "influences");
	}

	if (mesh->renderable) {
		/* Cargar el archivo index- */
		snprintf (name, sizeof (name), "index-%i", mesh->id);

		read_indices (folder, name);
	}
}

int read_vector_of_numbers (float **array, int fd, int encoding) {
	off_t len;
	int g;
	uint8_t u8;
	uint16_t u16;
	uint32_t u32;
	int8_t s8;
	int16_t s16;
	int32_t s32;
	float *pf, f;

	if (array == NULL) return 0;

	len = lseek (fd, 0, SEEK_END);

	lseek (fd, 0, SEEK_SET);

	if (encoding == -1) {
		TRY_READ_OR_GOTO (fd, &u8, 1, error_vector_of_number);
		encoding = u8;
		len = len - 1;
	}
	switch (encoding) {
		case ENCODING_NONE:
			len = len / 4;
			break;
		case ENCODING_BYTE:
		case ENCODING_BYTE_SIGNED:
		case UNENCODED_BYTE:
		case UNENCODED_BYTE_SIGNED:
			break;
		case ENCODING_SHORT:
		case ENCODING_SHORT_SIGNED:
		case UNENCODED_SHORT:
		case UNENCODED_SHORT_SIGNED:
			len = len / 2;
			break;
	}

	*array = (float *) malloc (sizeof (float) * len);

	if (*array == NULL) {
		return 0;
	}

	for (g = 0; g < len; g++) {
		pf = &f;
		switch (encoding) {
			case ENCODING_NONE:
				TRY_READ_OR_GOTO (fd, &u32, 4, error_vector_of_number);
				u32 = endian_32 (u32);
				pf = (float *) &u32;
				break;
			case ENCODING_BYTE:
				TRY_READ_OR_GOTO (fd, &u8, 1, error_vector_of_number);
				f = ((float) u8) / ((float) 255.0);
				break;
			case ENCODING_BYTE_SIGNED:
				TRY_READ_OR_GOTO (fd, &s8, 1, error_vector_of_number);
				f = ((float) s8) / ((float) 127.0);
				break;
			case UNENCODED_BYTE:
				TRY_READ_OR_GOTO (fd, &u8, 1, error_vector_of_number);
				f = (float) u8;
				break;
			case UNENCODED_BYTE_SIGNED:
				TRY_READ_OR_GOTO (fd, &s8, 1, error_vector_of_number);
				f = (float) s8;
				break;
			case ENCODING_SHORT:
				TRY_READ_OR_GOTO (fd, &u16, 2, error_vector_of_number);
				f = ((float) u16) / ((float) 65535.0);
				break;
			case ENCODING_SHORT_SIGNED:
				TRY_READ_OR_GOTO (fd, &s16, 2, error_vector_of_number);
				f = ((float) s16) / ((float) 32767.0);
				break;
			case UNENCODED_SHORT:
				TRY_READ_OR_GOTO (fd, &u16, 2, error_vector_of_number);
				f = (float) u16;
				break;
			case UNENCODED_SHORT_SIGNED:
				TRY_READ_OR_GOTO (fd, &s16, 2, error_vector_of_number);
				f = (float) s16;
				break;
		}

		(*array)[g] = *pf;
	}

	return len;

error_vector_of_number:
	if (*array != NULL) free (*array);
	return 0;
}

void read_vertex_data (Table *table, char *folder) {
	unsigned char buffer[8192];
	int fd_vertex;
	uint8_t u8;
	int8_t s8;
	uint16_t u16;
	uint32_t u32;
	int g, h;

	int id;
	float *vertex;

	id = get_key_as_int (table, "id");
	snprintf (buffer, sizeof (buffer), "%s/vertex-%i", folder, id);

	fd_vertex = open (buffer, O_RDONLY);

	if (fd_vertex < 0) {
		return;
	}

	u32 = read_vector_of_numbers (&vertex, fd_vertex, ENCODING_NONE);

	for (g = 0; g < u32; g = g + 3) {
		printf ("Vertex: %.8f, %.8f, %.8f\n", vertex[g], vertex[g + 1], vertex[g + 2]);
	}

	close (fd_vertex);
	return;
error_index:
	printf ("Skipping read vertex %i....\n", id);
	close (fd_vertex);
}

int main (int argc, char *argv[]) {
	BKVDesc bkv_desc, color_0;

	char *folder = "C:\\Users\\User\\Desktop\\CP APP\\penguin\\"; //Edit only this part

	read_bkv (&bkv_desc, folder, "desc");

	printf ("DESC: Valores tabla raíz:\n");

	printf ("{\n");
	print_table (&bkv_desc.tables[0], "\t");
	printf ("}\n");

	read_transform (&bkv_desc, folder);

	read_skeleton (&bkv_desc, folder);


	read_bkv (&color_0, folder, "Color-0.bkv");

	printf ("Color-0: Valores tabla raíz:\n");

	printf ("{\n");
	print_table (&color_0.tables[0], "\t");
	printf ("}\n");

	/* Procesar los vextex datas */
	Table *vertex_table, *t;
	int total, g;

	vertex_table = get_key_as_table (&bkv_desc.tables[0], "vertexDatas");

	if (vertex_table != NULL) {
		total = get_num_values (vertex_table);

		for (g = 0; g < total; g++) {
			t = get_index_as_table (vertex_table, g);

			/* TODO: Revisar si el nombre de este mesh es un "BlendShape" */
			read_vertex_data (t, folder);
		}
	}

	/* Procesar los meshes */
	Table *meshes_tables;
	MeshData mesh;

	meshes_tables = get_key_as_table (&bkv_desc.tables[0], "meshes");

	if (meshes_tables != NULL) {
		total = get_num_values (meshes_tables);

		for (g = 0; g < total; g++) {
			t = get_index_as_table (meshes_tables, g);

			/* TODO: Revisar si el nombre de este mesh es un "BlendShape" */
			load_mesh_data (&mesh, t, folder);
		}
	}
	return 0;
}

