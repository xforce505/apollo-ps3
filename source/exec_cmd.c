#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sysutil/sysutil.h>

#include "saves.h"
#include "menu.h"
#include "common.h"
#include "util.h"
#include "pfd.h"
#include "sfo.h"


void downloadSave(const char* file, const char* path)
{
	if (mkdirs(path) != SUCCESS)
	{
		show_message("Error! Export folder is not available");
		return;
	}

	if (http_download(ONLINE_URL "PS3/", file, APOLLO_LOCAL_CACHE "tmpsave.zip", 1))
	{
		if (extract_zip(APOLLO_LOCAL_CACHE "tmpsave.zip", path))
			show_message("Save game successfully downloaded");
		else
			show_message("Error extracting save game!");

		unlink_secure(APOLLO_LOCAL_CACHE "tmpsave.zip");
	}
	else
		show_message("Error downloading save game!");
}

void _saveOwnerData(const char* path)
{
	char buff[SYSUTIL_SYSTEMPARAM_CURRENT_USERNAME_SIZE+1];

	sysUtilGetSystemParamString(SYSUTIL_SYSTEMPARAM_ID_CURRENT_USERNAME, buff, SYSUTIL_SYSTEMPARAM_CURRENT_USERNAME_SIZE);
	LOG("Saving User '%s'...", buff);
	save_xml_owner(path, buff);
}

uint32_t get_filename_id(const char* dir)
{
	char path[128];
	uint32_t tid = 0;

	do
	{
		tid++;
		snprintf(path, sizeof(path), "%s%08d.zip", dir, tid);
	}
	while (file_exists(path) == SUCCESS);

	return tid;
}

void zipSave(const char* save_path, const char* exp_path)
{
	char* export_file;
	char* tmp;
	uint32_t fid;

	if (mkdirs(exp_path) != SUCCESS)
	{
		show_message("Error! Export folder is not available");
		return;
	}

	init_loading_screen("Exporting save game...");

	fid = get_filename_id(exp_path);
	asprintf(&export_file, "%s%08d.zip", exp_path, fid);

	asprintf(&tmp, save_path);
	*strrchr(tmp, '/') = 0;
	*strrchr(tmp, '/') = 0;

	zip_directory(tmp, save_path, export_file);

	sprintf(export_file, "%s" "saves.txt", exp_path);
	FILE* f = fopen(export_file, "a");
	if (f)
	{
		fprintf(f, "%08d.zip=[%s]%s\n", fid, selected_entry->title_id, selected_entry->name);
		fclose(f);
	}

	sprintf(export_file, "%s" OWNER_XML_FILE, exp_path);
	_saveOwnerData(export_file);

	free(export_file);
	free(tmp);

	stop_loading_screen();
}

void copySave(const char* save_path, const char* exp_path)
{
	char* copy_path;
	char* tmp;

	if (strncmp(save_path, exp_path, strlen(exp_path)) == 0)
		return;

	if (mkdirs(exp_path) != SUCCESS)
	{
		show_message("Error! Export folder is not available");
		return;
	}

	init_loading_screen("Copying save game...");

	asprintf(&tmp, save_path);
	*strrchr(tmp, '/') = 0;
	asprintf(&copy_path, "%s%s/", exp_path, strrchr(tmp, '/')+1);

	LOG("Copying <%s> to %s...", save_path, copy_path);
	copy_directory(save_path, save_path, copy_path);

	free(copy_path);
	free(tmp);

	stop_loading_screen();
}

void copySaveHDD(const char* save_path)
{
	char* copy_path;
	char* tmp;
	const char* folder;

	asprintf(&tmp, save_path);
	*strrchr(tmp, '/') = 0;
	folder = strrchr(tmp, '/')+1;
	asprintf(&copy_path, SAVES_PATH_HDD "%s/", apollo_config.user_id, folder);

	if (dir_exists(copy_path) == SUCCESS)
	{
		show_message("Error! Save-game folder already exists");
		free(copy_path);
		free(tmp);
		return;
	}

	init_loading_screen("Copying save game...");

	if (create_savegame_folder(folder))
	{
		LOG("Copying <%s> to %s...", save_path, copy_path);
		copy_directory(save_path, save_path, copy_path);
	}

	free(copy_path);
	free(tmp);

	stop_loading_screen();
}

void copyAllSavesHDD(const char* path)
{
	DIR *d;
	struct dirent *dir;
	char savePath[256];

	if (dir_exists(path) != SUCCESS)
	{
		LOG("Folder not available: %s", path);
		show_message("Error! Folder is not available");
		return;
	}

	d = opendir(path);
	if (!d)
		return;

	LOG("Copying all saves from '%s'...", path);
	while ((dir = readdir(d)) != NULL)
	{
		if (dir->d_type == DT_DIR && strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0)
		{
			snprintf(savePath, sizeof(savePath), "%s%s/PARAM.SFO", path, dir->d_name);
			if (file_exists(savePath) == SUCCESS)
			{
				snprintf(savePath, sizeof(savePath), "%s%s/", path, dir->d_name);
				copySaveHDD(savePath);
			}
		}
	}
	closedir(d);
}

void exportLicensesZip(const char* exp_path)
{
	char* export_file;
	char* lic_path;
	char* tmp;

	if (mkdirs(exp_path) != SUCCESS)
	{
		show_message("Error! Export folder is not available");
		return;
	}

	init_loading_screen("Exporting user licenses...");

	asprintf(&export_file, "%s" "licenses_%08d.zip", exp_path, apollo_config.user_id);
	asprintf(&lic_path, EXDATA_PATH_HDD, apollo_config.user_id);

	asprintf(&tmp, lic_path);
	*strrchr(tmp, '/') = 0;

	zip_directory(tmp, lic_path, export_file);

	sprintf(export_file, "%s" OWNER_XML_FILE, exp_path);
	_saveOwnerData(export_file);

	sprintf(export_file, "%s" "idps.hex", exp_path);
	write_buffer(export_file, (u8*) apollo_config.idps, 16);

	free(export_file);
	free(lic_path);
	free(tmp);

	stop_loading_screen();
}

void exportFlashZip(const char* exp_path)
{
	char* export_file;

	if (mkdirs(exp_path) != SUCCESS)
	{
		show_message("Error! Export folder is not available");
		return;
	}

	init_loading_screen("Exporting /dev_flash2.zip ...");

	asprintf(&export_file, "%s" "dev_flash2.zip", exp_path);
	zip_directory("/dev_flash2", "/dev_flash2/", export_file);

	sprintf(export_file, "%s" OWNER_XML_FILE, exp_path);
	_saveOwnerData(export_file);

	sprintf(export_file, "%s" "idps.hex", exp_path);
	write_buffer(export_file, (u8*) apollo_config.idps, 16);

	free(export_file);

	stop_loading_screen();
}

void resignPSVfile(const char* psv_path)
{
	init_loading_screen("Resigning PSV file...");
	psv_resign(psv_path);
	stop_loading_screen();
}

void copyDummyPSV(const char* psv_file, const char* out_path)
{
	char *in, *out;

	if (mkdirs(out_path) != SUCCESS)
	{
		show_message("Error! Export folder is not available");
		return;
	}

	asprintf(&in, APOLLO_DATA_PATH "%s", psv_file);
	asprintf(&out, "%s%s", out_path, psv_file);

	init_loading_screen("Copying PSV file...");
	copy_file(in, out);
	stop_loading_screen();

	free(in);
	free(out);
}

void exportPSVfile(const char* in_file, const char* out_path)
{
	if (mkdirs(out_path) != SUCCESS)
	{
		show_message("Error! Export folder is not available");
		return;
	}

	init_loading_screen("Exporting PSV file...");

	if (selected_entry->flags & SAVE_FLAG_PS1)
		ps1_psv2mcs(in_file, out_path);
	else
		ps2_psv2psu(in_file, out_path);

	stop_loading_screen();
}

void convertSavePSV(const char* save_path, const char* out_path, uint16_t type)
{
	if (mkdirs(out_path) != SUCCESS)
	{
		show_message("Error! Export folder is not available");
		return;
	}

	init_loading_screen("Converting Save to PSV file...");

	switch (type)
	{
	case FILE_TYPE_MCS:
		ps1_mcs2psv(save_path, out_path);
		break;

	case FILE_TYPE_PSX:
		ps1_psx2psv(save_path, out_path);
		break;

	case FILE_TYPE_MAX:
		ps2_max2psv(save_path, out_path);
		break;

	case FILE_TYPE_PSU:
		ps2_psu2psv(save_path, out_path);
		break;

	case FILE_TYPE_CBS:
		ps2_cbs2psv(save_path, out_path);
		break;

	case FILE_TYPE_XPS:
		ps2_xps2psv(save_path, out_path);
		break;

	default:
		break;
	}

	stop_loading_screen();
}

void decryptVMEfile(const char* vme_path, const char* vme_file, uint8_t dst)
{
	char vmefile[256];
	char outfile[256];
	const char *path;

	switch (dst)
	{
	case 0:
		path = EXP_PS2_PATH_USB0;
		break;

	case 1:
		path = EXP_PS2_PATH_USB1;
		break;

	case 2:
		path = EXP_PS2_PATH_HDD;
		break;

	default:
		return;
	}

	if (mkdirs(path) != SUCCESS)
	{
		show_message("Error! Export folder is not available");
		return;
	}

	snprintf(vmefile, sizeof(vmefile), "%s%s", vme_path, vme_file);
	snprintf(outfile, sizeof(outfile), "%sAPOLLO%c.VM2", path, vme_file[6]);

	init_loading_screen("Decrypting VME card...");
	ps2_crypt_vmc(0, vmefile, outfile, 0);
	stop_loading_screen();
}

void encryptVM2file(const char* vme_path, const char* vme_file, const char* src_name)
{
	char vmefile[256];
	char srcfile[256];

	snprintf(vmefile, sizeof(vmefile), "%s%s", vme_path, vme_file);
	snprintf(srcfile, sizeof(srcfile), "%s%s", EXP_PS2_PATH_HDD, src_name);

	init_loading_screen("Encrypting VM2 card...");
	ps2_crypt_vmc(0, srcfile, vmefile, 1);
	stop_loading_screen();
}

void importPS2VMC(const char* vmc_path, const char* vmc_file)
{
	char vm2file[256];
	char srcfile[256];

	snprintf(srcfile, sizeof(srcfile), "%s%s", vmc_path, vmc_file);
	snprintf(vm2file, sizeof(vm2file), "%s%s", EXP_PS2_PATH_HDD, vmc_file);
	strcpy(strrchr(vm2file, '.'), ".VM2");

	init_loading_screen("Importing PS2 memory card...");
	ps2_add_vmc_ecc(srcfile, vm2file);
	stop_loading_screen();
}

void exportVM2raw(const char* vm2_path, const char* vm2_file, const char* dst_path)
{
	char vm2file[256];
	char dstfile[256]; 

	if (mkdirs(dst_path) != SUCCESS)
	{
		show_message("Error! Export folder is not available");
		return;
	}

	snprintf(vm2file, sizeof(vm2file), "%s%s", vm2_path, vm2_file);
	snprintf(dstfile, sizeof(dstfile), "%s%s.vmc", dst_path, vm2_file);

	init_loading_screen("Exporting PS2 .VM2 memory card...");
	ps2_remove_vmc_ecc(vm2file, dstfile);
	stop_loading_screen();
}

void importPS2classics(const char* iso_path, const char* iso_file)
{
	char ps2file[256];
	char outfile[256];
	char msg[128] = "Encrypting PS2 ISO...";

	snprintf(ps2file, sizeof(ps2file), "%s%s", iso_path, iso_file);
	snprintf(outfile, sizeof(outfile), IMPORT_PS2_PATH_HDD "%s", iso_file);
	*strrchr(outfile, '.') = 0;
	strcat(outfile, "/ISO.BIN.ENC");

	if (mkdirs(outfile) != SUCCESS)
	{
		show_message("Error! Export folder is not available");
		return;
	}

	init_loading_screen(msg);
	ps2_encrypt_image(0, ps2file, outfile, msg);
	stop_loading_screen();
}

void exportPS2classics(const char* enc_path, const char* enc_file, uint8_t dst)
{
	char ps2file[256];
	char outfile[256];
	char msg[128] = "Decrypting PS2 BIN.ENC...";
	const char *path;

	switch (dst)
	{
	case 0:
		path = IMPORT_PS2_PATH_USB0;
		break;

	case 1:
		path = IMPORT_PS2_PATH_USB1;
		break;

	case 2:
		path = IMPORT_PS2_PATH_HDD;
		break;

	default:
		return;
	}

	snprintf(ps2file, sizeof(ps2file), "%s%s", enc_path, enc_file);
	snprintf(outfile, sizeof(outfile), "%s%s", path, enc_file);
	*strrchr(outfile, '.') = 0;
	strcat(outfile, ".ps2.iso");

	if (mkdirs(outfile) != SUCCESS)
	{
		show_message("Error! Export folder is not available");
		return;
	}

	init_loading_screen(msg);
	ps2_decrypt_image(0, ps2file, outfile, msg);
	stop_loading_screen();
}


void exportFolder(const char* src_path, const char* exp_path, const char* msg)
{
	char* save_path;

	if (mkdirs(exp_path) != SUCCESS)
	{
		show_message("Error! Export folder is not available");
		return;
	}

	init_loading_screen(msg);

	asprintf(&save_path, src_path, apollo_config.user_id);

    LOG("Copying <%s> to %s...", save_path, exp_path);
	copy_directory(save_path, save_path, exp_path);

	free(save_path);

	stop_loading_screen();
}

void exportLicensesRap(const char* fname, uint8_t dest)
{
	DIR *d;
	struct dirent *dir;
	char lic_path[256];
	char msg[128] = "Exporting user licenses...";
	const char* exp_path;

	switch (dest)
	{
	case 0:
		exp_path = EXPORT_RAP_PATH_USB0;
		break;
	
	case 1:
		exp_path = EXPORT_RAP_PATH_USB1;
		break;

	case 2:
		exp_path = EXPORT_RAP_PATH_HDD;
		break;

	default:
		return;
	}

	if (mkdirs(exp_path) != SUCCESS)
	{
		show_message("Error! Export folder is not available");
		return;
	}

	snprintf(lic_path, sizeof(lic_path), EXDATA_PATH_HDD, apollo_config.user_id);
	d = opendir(lic_path);
	if (!d)
		return;

    init_loading_screen(msg);

	LOG("Exporting RAPs from folder '%s'...", lic_path);
	while ((dir = readdir(d)) != NULL)
	{
		if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0 &&
			(!fname || (strcmp(dir->d_name, fname) == 0)) &&
			strcmp(strrchr(dir->d_name, '.'), ".rif") == 0)
		{
			LOG("Exporting %s", dir->d_name);
			snprintf(msg, sizeof(msg), "Exporting %.36s...", dir->d_name);
			rif2rap((u8*) apollo_config.idps, lic_path, dir->d_name, exp_path);
		}
	}
	closedir(d);

    stop_loading_screen();
}

void importLicenses(const char* fname, const char* exdata_path)
{
	DIR *d;
	struct dirent *dir;
	char lic_path[256];

	if (dir_exists(exdata_path) != SUCCESS)
	{
		LOG("Folder not available: %s", exdata_path);
		show_message("Error! Import folder is not available");
		return;
	}

	snprintf(lic_path, sizeof(lic_path), EXDATA_PATH_HDD, apollo_config.user_id);
	d = opendir(exdata_path);
	if (!d)
		return;

    init_loading_screen("Importing user licenses...");

	LOG("Importing RAPs from folder '%s'...", exdata_path);
	while ((dir = readdir(d)) != NULL)
	{
		if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0 &&
			(!fname || (strcmp(dir->d_name, fname)) == 0) &&
			strcmp(strrchr(dir->d_name, '.'), ".rap") == 0)
		{
			LOG("Importing %s", dir->d_name);
			rap2rif((u8*) apollo_config.idps, exdata_path, dir->d_name, lic_path);
		}
	}
	closedir(d);

    stop_loading_screen();
}

int apply_sfo_patches(sfo_patch_t* patch)
{
    code_entry_t* code;
    char in_file_path[256];
    char tmp_dir[SFO_DIRECTORY_SIZE];
    u8 tmp_psid[SFO_PSID_SIZE];
    int j;

	for (j = 0; j < selected_entry->code_count; j++)
	{
		code = &selected_entry->codes[j];
		if (!code->activated || code->type != PATCH_SFO)
		    continue;

        LOG("Active: [%s]", code->name);

        switch (code->codes[0])
        {
        case SFO_UNLOCK_COPY:
            if (selected_entry->flags & SAVE_FLAG_LOCKED)
                selected_entry->flags ^= SAVE_FLAG_LOCKED;

            patch->flags = SFO_PATCH_FLAG_REMOVE_COPY_PROTECTION;
            break;

        case SFO_CHANGE_ACCOUNT_ID:
            if (selected_entry->flags & SAVE_FLAG_OWNER)
                selected_entry->flags ^= SAVE_FLAG_OWNER;

            memcpy(patch->account_id, code->options->value[code->options->sel], SFO_ACCOUNT_ID_SIZE);
            break;

        case SFO_REMOVE_PSID:
            bzero(tmp_psid, SFO_PSID_SIZE);
            patch->psid = tmp_psid;
            break;

        case SFO_CHANGE_TITLE_ID:
            patch->directory = strstr(selected_entry->path, selected_entry->title_id);
            snprintf(in_file_path, sizeof(in_file_path), "%s", selected_entry->path);
            strncpy(tmp_dir, patch->directory, SFO_DIRECTORY_SIZE);

            strncpy(selected_entry->title_id, code->options[0].name[code->options[0].sel], 9);
            strncpy(patch->directory, selected_entry->title_id, 9);
            strncpy(tmp_dir, selected_entry->title_id, 9);
            *strrchr(tmp_dir, '/') = 0;
            patch->directory = tmp_dir;

            LOG("Moving (%s) -> (%s)", in_file_path, selected_entry->path);
            rename(in_file_path, selected_entry->path);
            break;

        default:
            break;
        }

		code->activated = 0;
	}

	snprintf(in_file_path, sizeof(in_file_path), "%s" "PARAM.SFO", selected_entry->path);
	LOG("Applying SFO patches '%s'...", in_file_path);

	return (patch_sfo(in_file_path, patch) == SUCCESS);
}

int _is_decrypted(list_t* list, const char* fname) {
	list_node_t *node = list_head(list);
	u8 *protected_file_id = get_secure_file_id(selected_entry->title_id, "UNPROTECTED");

	if (protected_file_id && (strncmp("UNPROTECTEDGAME", (char*)protected_file_id, 16) == 0))
		return 1;

	while (node) {
		if (strcmp(list_get(node), fname) == 0)
			return 1;

		node = node->next;
	}

	return 0;
}

int apply_cheat_patches()
{
    int j, ret = 1;
	char tmpfile[256];
	char* filename;
	code_entry_t* code;
	uint8_t* protected_file_id;
	list_t* decrypted_files = list_alloc();

	init_loading_screen("Applying changes...");

	for (j = 0; j < selected_entry->code_count; j++)
	{
		code = &selected_entry->codes[j];

		if (!code->activated || (code->type != PATCH_GAMEGENIE && code->type != PATCH_BSD))
		    continue;

    	LOG("Active code: [%s]", code->name);

		if (strrchr(code->file, '\\'))
			filename = strrchr(code->file, '\\')+1;
		else
			filename = code->file;

		if (strchr(filename, '*'))
			filename = code->options[0].name[code->options[0].sel];

		if (strstr(code->file, "~extracted\\"))
			snprintf(tmpfile, sizeof(tmpfile), "%s[%s]%s", APOLLO_LOCAL_CACHE, selected_entry->title_id, filename);
		else
		{
			snprintf(tmpfile, sizeof(tmpfile), "%s%s", selected_entry->path, filename);

			if (!_is_decrypted(decrypted_files, filename))
			{
				LOG("Decrypting '%s'...", filename);

				protected_file_id = get_secure_file_id(selected_entry->title_id, filename);

				if (decrypt_save_file(selected_entry->path, filename, NULL, protected_file_id))
				{
					list_append(decrypted_files, strdup(filename));
				}
				else
				{
					LOG("Error: failed to decrypt (%s)", filename);
					ret = 0;
					continue;
				}
			}
		}

		if (!apply_cheat_patch_code(tmpfile, selected_entry->title_id, code))
		{
			LOG("Error: failed to apply (%s)", code->name);
			ret = 0;
		}

		code->activated = 0;
	}

	list_node_t *node = list_head(decrypted_files);

	while (node)
	{
		filename = list_get(node);
		snprintf(tmpfile, sizeof(tmpfile), "%s%s", selected_entry->path, filename);

		LOG("Encrypting '%s'...", tmpfile);

		protected_file_id = get_secure_file_id(selected_entry->title_id, filename);
		
		if (!encrypt_save_file(selected_entry->path, filename, NULL, protected_file_id))
		{
			LOG("Error: failed to encrypt (%s)", tmpfile);
			ret = 0;
		}

		free(filename);
		node = node->next;
	}

	list_free(decrypted_files);
	stop_loading_screen();

	return ret;
}

void resignSave(sfo_patch_t* patch)
{
    if (!apply_sfo_patches(patch))
        show_message("Error! Account changes couldn't be applied");

    LOG("Applying cheats to '%s'...", selected_entry->name);
    if (!apply_cheat_patches())
        show_message("Error! Cheat codes couldn't be applied");

    LOG("Resigning save '%s'...", selected_entry->name);
    if (!pfd_util_init((u8*) apollo_config.psid, apollo_config.user_id, selected_entry->title_id, selected_entry->path) ||
        (pfd_util_process(PFD_CMD_UPDATE, 0) != SUCCESS))
        show_message("Error! Save file couldn't be resigned");
    else
        show_message("Save file successfully modified!");

    pfd_util_end();
}

void resignAllSaves(const char* path)
{
	DIR *d;
	struct dirent *dir;
	char sfoPath[256];
	char titleid[10];
	char acct_id[17];
	char message[128] = "Resigning all saves...";

	if (dir_exists(path) != SUCCESS)
	{
		LOG("Folder not available: %s", path);
		show_message("Error! Folder is not available");
		return;
	}

	d = opendir(path);
	if (!d)
		return;

    init_loading_screen(message);

	snprintf(acct_id, sizeof(acct_id), "%016lx", apollo_config.account_id);
	sfo_patch_t patch = {
		.flags = SFO_PATCH_FLAG_REMOVE_COPY_PROTECTION,
		.user_id = apollo_config.user_id,
		.psid = (u8*) apollo_config.psid,
		.account_id = acct_id,
		.directory = NULL,
	};

	LOG("Resigning saves from '%s'...", path);
	while ((dir = readdir(d)) != NULL)
	{
		if (dir->d_type == DT_DIR && strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0)
		{
			snprintf(sfoPath, sizeof(sfoPath), "%s%s/PARAM.SFO", path, dir->d_name);
			if (file_exists(sfoPath) == SUCCESS)
			{
				LOG("Patching SFO '%s'...", sfoPath);
				if (patch_sfo(sfoPath, &patch) != SUCCESS)
					continue;

				snprintf(titleid, sizeof(titleid), "%.9s", dir->d_name);
				snprintf(sfoPath, sizeof(sfoPath), "%s%s/", path, dir->d_name);
				snprintf(message, sizeof(message), "Resigning %s...", dir->d_name);

				LOG("Resigning save '%s'...", sfoPath);
				if (!pfd_util_init((u8*) apollo_config.psid, apollo_config.user_id, titleid, sfoPath) ||
					(pfd_util_process(PFD_CMD_UPDATE, 0) != SUCCESS))
					LOG("Error! Save file couldn't be resigned");

				pfd_util_end();
			}
		}
	}
	closedir(d);

	stop_loading_screen();
}

void decryptSaveFile(const char* filename)
{
	if (_is_decrypted(NULL, filename))
	{
		show_message("Save-game is not encrypted. No files decrypted");
		return;
	}

	u8* protected_file_id = get_secure_file_id(selected_entry->title_id, filename);

	LOG("Decrypt '%s' from '%s'...", filename, selected_entry->name);

	if (decrypt_save_file(selected_entry->path, filename, "/dev_hdd0/tmp/", protected_file_id))
		show_message("File successfully decrypted to /dev_hdd0/tmp/");
	else
		show_message("Error! File couldn't be decrypted");
}

void execCodeCommand(code_entry_t* code, const char* codecmd)
{
	switch (codecmd[0])
	{
		case CMD_DECRYPT_FILE:
			decryptSaveFile(code->options[0].name[code->options[0].sel]);
			code->activated = 0;
			break;

		case CMD_DOWNLOAD_USB:
			downloadSave(code->codes, codecmd[1] ? SAVES_PATH_USB1 : SAVES_PATH_USB0);
			code->activated = 0;
			break;

		case CMD_EXPORT_ZIP_USB:
			zipSave(selected_entry->path, codecmd[1] ? EXPORT_PATH_USB1 : EXPORT_PATH_USB0);
			code->activated = 0;
			break;

		case CMD_EXPORT_ZIP_HDD:
			zipSave(selected_entry->path, "/dev_hdd0/tmp/");
			code->activated = 0;
			break;

		case CMD_COPY_SAVE_USB:
			copySave(selected_entry->path, codecmd[1] ? SAVES_PATH_USB1 : SAVES_PATH_USB0);
			code->activated = 0;
			break;

		case CMD_COPY_SAVE_HDD:
			copySaveHDD(selected_entry->path);
			code->activated = 0;
			break;

		case CMD_EXP_EXDATA_USB:
			exportLicensesZip(codecmd[1] ? EXPORT_PATH_USB1 : EXPORT_PATH_USB0);
			code->activated = 0;
			break;

		case CMD_EXP_LICS_RAPS:
			exportLicensesRap(code->file, codecmd[1]);
			code->activated = 0;
			break;

		case CMD_EXP_TROPHY_USB:
			exportFolder(TROPHY_PATH_HDD, codecmd[1] ? EXPORT_PATH_USB1 : EXPORT_PATH_USB0, "Copying trophies...");
			code->activated = 0;
			break;

		case CMD_COPY_SAVES_USB:
			exportFolder(SAVES_PATH_HDD, codecmd[1] ? SAVES_PATH_USB1 : SAVES_PATH_USB0, "Copying saves...");
			code->activated = 0;
			break;

		case CMD_EXP_FLASH2_USB:
			exportFlashZip(codecmd[1] ? EXPORT_PATH_USB1 : EXPORT_PATH_USB0);
			code->activated = 0;
			break;

		case CMD_IMP_EXDATA_USB:
			importLicenses(code->file, selected_entry->path);
			code->activated = 0;
			break;

		case CMD_RESIGN_SAVE:
			{
				sfo_patch_t patch = {
					.flags = 0,
					.user_id = apollo_config.user_id,
					.psid = (u8*) apollo_config.psid,
					.directory = NULL,
				};
				asprintf(&patch.account_id, "%016lx", apollo_config.account_id);

				resignSave(&patch);
				free(patch.account_id);
			}
			code->activated = 0;
			break;

		case CMD_RESIGN_SAVES_USB:
			resignAllSaves(selected_entry->path);
			code->activated = 0;
			break;

		case CMD_COPY_SAVES_HDD:
			copyAllSavesHDD(selected_entry->path);
			code->activated = 0;
			break;

		case CMD_RESIGN_PSV:
			resignPSVfile(selected_entry->path);
			code->activated = 0;
			break;

		case CMD_DECRYPT_PS2_VME:
			decryptVMEfile(selected_entry->path, code->file, codecmd[1]);
			code->activated = 0;
			break;

		case CMD_ENCRYPT_PS2_VMC:
			encryptVM2file(selected_entry->path, code->file, code->options[0].name[code->options[0].sel]);
			code->activated = 0;
			break;

		case CMD_IMP_PS2ISO_USB:
			importPS2classics(selected_entry->path, code->file);
			code->activated = 0;
			break;

		case CMD_CONVERT_TO_PSV:
			convertSavePSV(selected_entry->path, codecmd[1] ? EXP_PSV_PATH_USB1 : EXP_PSV_PATH_USB0, selected_entry->type);
			code->activated = 0;
			break;

		case CMD_COPY_DUMMY_PSV:
			copyDummyPSV(code->file, codecmd[1] ? EXP_PSV_PATH_USB1 : EXP_PSV_PATH_USB0);
			code->activated = 0;
			break;

		case CMD_EXP_PS2_BINENC:
			exportPS2classics(selected_entry->path, code->file, codecmd[1]);
			code->activated = 0;
			break;

		case CMD_EXP_PSV_MCS:
			exportPSVfile(selected_entry->path, codecmd[1] ? USB1_PATH PS1_IMP_PATH_USB : USB0_PATH PS1_IMP_PATH_USB);
			code->activated = 0;
			break;

		case CMD_EXP_PSV_PSU:
			exportPSVfile(selected_entry->path, codecmd[1] ? USB1_PATH PS2_IMP_PATH_USB : USB0_PATH PS2_IMP_PATH_USB);
			code->activated = 0;
			break;

		case CMD_EXP_VM2_RAW:
			exportVM2raw(selected_entry->path, code->file, codecmd[1] ? EXP_PS2_PATH_USB1 : EXP_PS2_PATH_USB0);
			code->activated = 0;
			break;

		case CMD_IMP_PS2VMC_USB:
			importPS2VMC(selected_entry->path, code->file);
			code->activated = 0;
			break;

		default:
			break;
	}

}
