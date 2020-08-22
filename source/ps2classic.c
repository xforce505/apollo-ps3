/*+-----------------------------+*/
/*|2013 USER                    |*/
/*|decrypt algos by flatz       |*/
/*|                             |*/
/*|GPL v3,  DO NOT USE IF YOU   |*/
/*|DISAGREE TO RELEASE SRC :P   |*/
/*+-----------------------------+*/

#include "ecdsa.h"
#include "types.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <polarssl/aes.h>
#include <polarssl/sha1.h>

#define PS2_META_SEGMENT_START      1
#define PS2_DATA_SEGMENT_START      2
#define PS2_DEFAULT_SEGMENT_SIZE    0x4000
#define PS2_META_ENTRY_SIZE         0x20
#define PS2_REAL_OUT_NAME           "ISO.BIN.ENC"
#define PS2_PLACEHOLDER_CID         "2P0001-PS2U10000_00-0000111122223333"

#define PS2_VMC_ENCRYPT             1
#define PS2_VMC_DECRYPT             0

#define be32(x)                     *(u32*)(x)
#define be64(x)                     *(u64*)(x)
#define set_ps2_iv(x)               memset(x, 0, 0x10)


//prototypes
static void build_ps2_header(u8 * buffer, int npd_type, const char* content_id, const char* filename, s64 iso_size);
int vmc_hash(const char *mc_in);
int ps2_iso9660_sig(FILE *f, const char *img_in);
void ps2_build_limg(const char *img_in, int64_t size);

//keys
u8 ps2_per_console_seed[16] = { 0xD9, 0x2D, 0x65, 0xDB, 0x05, 0x7D, 0x49, 0xE1, 0xA6, 0x6F, 0x22, 0x74, 0xB8, 0xBA, 0xC5, 0x08 };

u8 ps2_key_cex_meta[16] = { 0x38, 0x9D, 0xCB, 0xA5, 0x20, 0x3C, 0x81, 0x59, 0xEC, 0xF9, 0x4C, 0x93, 0x93, 0x16, 0x4C, 0xC9 };
u8 ps2_key_cex_data[16] = { 0x10, 0x17, 0x82, 0x34, 0x63, 0xF4, 0x68, 0xC1, 0xAA, 0x41, 0xD7, 0x00, 0xB1, 0x40, 0xF2, 0x57 };
u8 ps2_key_cex_vmc[16]  = { 0x64, 0xE3, 0x0D, 0x19, 0xA1, 0x69, 0x41, 0xD6, 0x77, 0xE3, 0x2E, 0xEB, 0xE0, 0x7F, 0x45, 0xD2 };

u8 ps2_key_dex_meta[16] = { 0x2B, 0x05, 0xF7, 0xC7, 0xAF, 0xD1, 0xB1, 0x69, 0xD6, 0x25, 0x86, 0x50, 0x3A, 0xEA, 0x97, 0x98 };
u8 ps2_key_dex_data[16] = { 0x74, 0xFF, 0x7E, 0x5D, 0x1D, 0x7B, 0x96, 0x94, 0x3B, 0xEF, 0xDC, 0xFA, 0x81, 0xFC, 0x20, 0x07 };
u8 ps2_key_dex_vmc[16]  = { 0x30, 0x47, 0x9D, 0x4B, 0x80, 0xE8, 0x9E, 0x2B, 0x59, 0xE5, 0xC9, 0x14, 0x5E, 0x10, 0x64, 0xA9 };

u8 ps2_placeholder_klic[16] = { 0xe4, 0xe5, 0x4f, 0xd6, 0x7c, 0x16, 0xc3, 0x16, 0xf4, 0x78, 0x29, 0xa3, 0x04, 0x84, 0xd8, 0x43 };

u8 fallback_header_hash[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };

u8 npd_omac_key2[16] = {0x6B,0xA5,0x29,0x76,0xEF,0xDA,0x16,0xEF,0x3C,0x33,0x9F,0xB2,0x97,0x1E,0x25,0x6B};
u8 npd_omac_key3[16] = {0x9B,0x51,0x5F,0xEA,0xCF,0x75,0x06,0x49,0x81,0xAA,0x60,0x4D,0x91,0xA5,0x4E,0x97};
u8 npd_kek[16]       = {0x72,0xF9,0x90,0x78,0x8F,0x9C,0xFF,0x74,0x57,0x25,0xF0,0x8E,0x4C,0x12,0x83,0x87};


void aes128cbc(const u8 *key, const u8 *iv_in, const u8 *in, u64 len, u8 *out)
{
	u8 iv[16];
	aes_context ctx;

	memcpy(iv, iv_in, 16);
	memset(&ctx, 0, sizeof(aes_context));
	aes_setkey_dec(&ctx, key, 128);
	aes_crypt_cbc(&ctx, AES_DECRYPT, len, iv, in, out);
}

void aes128cbc_enc(const u8 *key, const u8 *iv_in, const u8 *in, u64 len, u8 *out)
{
	u8 iv[16];
	aes_context ctx;

	memcpy(iv, iv_in, 16);
	memset(&ctx, 0, sizeof(aes_context));
	aes_setkey_enc(&ctx, key, 128);
	aes_crypt_cbc(&ctx, AES_ENCRYPT, len, iv, in, out);
}

void rol1(u8* worthless) {
	int i;
	u8 xor = (worthless[0]&0x80)?0x87:0;

	for(i=0;i<0xF;i++) {
		worthless[i] <<= 1;
		worthless[i] |= worthless[i+1]>>7;
	}
	worthless[0xF] <<= 1;
	worthless[0xF] ^= xor;
}

void aesOmacMode1(u8* output, u8* input, int len, u8* aes_key_data, int aes_key_bits)
{
	int i,j;
	i = 0;
	u8 running[0x10];
	u8 hash[0x10];
	u8 worthless[0x10];
	aes_context ctx;

	memset(running, 0, 0x10);
	memset(&ctx, 0, sizeof(aes_context));

	aes_setkey_enc(&ctx, aes_key_data, aes_key_bits);
	aes_crypt_ecb(&ctx, AES_ENCRYPT, running, worthless);
	rol1(worthless);

	if(len > 0x10) {
		for(i=0;i<(len-0x10);i+=0x10) {
			for(j=0;j<0x10;j++) hash[j] = running[j] ^ input[i+j];
			aes_crypt_ecb(&ctx, AES_ENCRYPT, hash, running);
		}
	}
	int overrun = len&0xF;
	if( (len%0x10) == 0 ) overrun = 0x10;

	memset(hash, 0, 0x10);
	memcpy(hash, &input[i], overrun);

	if(overrun != 0x10) {
		hash[overrun] = 0x80;
		rol1(worthless);
	}

	for(j=0;j<0x10;j++) hash[j] ^= running[j] ^ worthless[j];
	aes_crypt_ecb(&ctx, AES_ENCRYPT, hash, output);
}

/*
 *  vmc proper hash
 */
int vmc_hash(const char *mc_in) {
    
    int i, segment_count = 0, TotalSize=0 ;
    uint8_t segment_hashes[16384], segment_data[16384], sha1_hash[0x14];
    u32 read = 0;
    
	int64_t c, mc_size;
	int percent;
	
	mc_size = c = percent = 0;
    
    memset(segment_hashes, 0, sizeof(segment_hashes));
    
    FILE *f = fopen(mc_in, "rb+");
    
    if(fseek(f, 0, SEEK_SET) == 0) {
        uint8_t buf[8];
        
        if(fread(buf, 1, 8, f)) {
            if (memcmp(buf, "Sony PS2", sizeof(buf)) != 0)
            {
                LOG("Not a valid Virtual Memory Card...:\n");
                
                fclose(f);
                return(0);
            }
        }
        
        fseeko(f, 0, SEEK_END);
        TotalSize = ftello(f);
        
        segment_count = (TotalSize - sizeof(segment_data)) / sizeof(segment_data);
        
        for (i = 0; i < segment_count; i++)
        {
            fseek(f, (i * sizeof(segment_data)), SEEK_SET);
            read = fread(segment_data, 1, sizeof(segment_data), f);
            //fwrite(segment_data, 1, sizeof(segment_data), out);
            sha1(segment_data, sizeof(segment_data), sha1_hash);
            memcpy(segment_hashes + i * sizeof(sha1_hash), sha1_hash, sizeof(sha1_hash));
            
            c += read;
            percent += 1;
            mc_size = mc_size + c;
            LOG("Rehash: Block %03d | Offset 0x%08lx", percent, mc_size);
            c = 0;
            
        }
        
        fseek(f, (segment_count * sizeof(segment_data)), SEEK_SET);
        fwrite(segment_hashes, 1, sizeof(segment_hashes), f);
        fclose(f);
        
        LOG("Done...");
    }
    return 1;
}

/*
 *  ps2_iso9660_sig
 */
int ps2_iso9660_sig(FILE *f, const char *img_in) {
    
	/* iso9660 offset DVD */
	if(fseek(f, 0x8000, SEEK_SET) == 0) {
		uint8_t buf[8];

		if(fread(buf, 1, 6, f)) {
			if(buf[0] != 0x01 && buf[1] != 0x43 && buf[2] != 0x44 && buf[3] != 0x30 && buf[4] != 0x30 && buf[5] != 0x31)
            {
                /* NOW TRY CD OFFSET */
                fseek(f, 0x9318, SEEK_SET);
                memset(buf, 0, sizeof(buf));
                                
                if(fread(buf, 1, 6, f)) {
                    if(buf[0] != 0x01 && buf[1] != 0x43 && buf[2] != 0x44 && buf[3] != 0x30 && buf[4] != 0x30 && buf[5] != 0x31) {
                        
                        LOG("Not a valid ISO9660 Image...:\n");
                        LOG(" input\t\t%s [ERROR]\n", img_in);
                        
                        fclose(f);
                        return(0);
                    }
                }
            }
        }

		fseek(f, 0, SEEK_SET);
	}
	return 1;
}

/*
 * ps2_build_limg
 */
void ps2_build_limg(const char *img_in, int64_t size) {
	int i, b_size = 0;
	uint8_t buf[8], tmp[8], num_sectors[8];
	int64_t tsize=0;

	FILE *f = fopen(img_in, "rb+");

	/* limg offset */
	if(fseek(f, -0x4000, SEEK_END) == 0) {
		if(fread(buf, 1, 4, f)) {
			/* skip */
			if(buf[0] == 0x4c && buf[1] == 0x49 && buf[2] == 0x4d && buf[3] == 0x47) {
				LOG("[*] LIMG Header:\n\t\t[OK]\n");
				fclose(f);
				return;
			}
		}
	}
	
	/* seek start */
	fseek(f, 0, SEEK_SET);
	
	// Get MSB Value size, DVD or CD
	if(size > 0x2BC00000) {
        fseek(f, 0x8000 + 0x54, SEEK_SET);
        fread(num_sectors, 1, 4, f);
    } else {
        fseek(f, 0x9318 + 0x54, SEEK_SET);
        fread(num_sectors, 1, 4, f);
    }

	for(i = 0; i < 8; i++)
		buf[i] = tmp[i] = 0x00;
	
	tsize = size;

	/* seek end */
	fseek(f, 0, SEEK_END);
	
	/* Check if image is multiple of 0x4000, if not then pad with zeros*/
	while((tsize%16384) != 0) {
        tsize++;
        fwrite(&buf[0], 1, 1, f);
    }
    
    LOG("Multiple of 0x4000 [OK]\n");

	if(size > 0x2BC00000)
		b_size = 0x800; /* dvd */
	else
		b_size = 0x930; /* cd */

	if(b_size == 0x800) {
		buf[3] = 0x01;
		tmp[2] = 0x08;
	} else {
		buf[3] = 0x02;
		tmp[2] = 0x09;
		tmp[3] = 0x30;
	}

	/* print output */
	LOG("[*] LIMG Header:\n 4C 49 4D 47 ");

	fwrite("LIMG", 1, 4, f);
	fwrite(buf, 1, 4, f);

	/* print output */
	for(i = 0; i < 4; i++)
		LOG("%02X ", buf[i]);

	/* write number of sectors*/
	fwrite(num_sectors, 1, 4, f);

	/* print output */
	for(i = 0; i < 4; i++)
		LOG("%02X ", num_sectors[i]);

	fwrite(tmp, 1, 4, f);

	/* print output */
	for(i = 0; i < 4; i++)
		LOG("%02X ", tmp[i]);

	LOG("\n");

	/* 0x4000 - 0x10 - Rellena con ceros la seccion final de 16384 bytes */
	for(i = 0; i < 0x3FF0; i++)
		fwrite(&buf[0], 1, 1, f);

	fclose(f);
}

void wbe16(u8* buf, u16 data)
{
	memcpy(buf, &data, sizeof(u16));
}

void wbe32(u8* buf, u32 data)
{
	memcpy(buf, &data, sizeof(u32));
}

void wbe64(u8* buf, u64 data)
{
	memcpy(buf, &data, sizeof(u64));
}

static void build_ps2_header(u8 * buffer, int npd_type, const char* content_id, const char* filename, s64 iso_size)
{
	int i;
	u32 type = 1;
//	u8 test_hash[] = { 0xBF, 0x2E, 0x44, 0x15, 0x52, 0x8F, 0xD7, 0xDD, 0xDB, 0x0A, 0xC2, 0xBF, 0x8C, 0x15, 0x87, 0x51 };

	wbe32(buffer, 0x50533200);			// PS2\0
	wbe16(buffer + 0x4, 0x1);			// ver major
	wbe16(buffer + 0x6, 0x1);			// ver minor
	wbe32(buffer + 0x8, npd_type);		// NPD type XX
	wbe32(buffer + 0xc, type);			// type
		
	wbe64(buffer + 0x88, iso_size); 		//iso size
	wbe32(buffer + 0x84, PS2_DEFAULT_SEGMENT_SIZE); //segment size
	
	strncpy((char*)(buffer + 0x10), content_id, 0x30);

	u8 npd_omac_key[0x10];

	for(i=0;i<0x10;i++) npd_omac_key[i] = npd_kek[i] ^ npd_omac_key2[i];

	get_rand(buffer + 0x40, 0x10); //npdhash1
	//memcpy(buffer + 0x40, test_hash, 0x10);

  	int buf_len = 0x30+strlen(filename);
	char *buf = (char*)malloc(buf_len+1);
	memcpy(buf, buffer + 0x10, 0x30);
	strcpy(buf+0x30, filename);
	aesOmacMode1(buffer + 0x50, (u8*)buf, buf_len, npd_omac_key3, sizeof(npd_omac_key3)*8);  //npdhash2
	free(buf);
	aesOmacMode1(buffer + 0x60, buffer, 0x60, npd_omac_key, sizeof(npd_omac_key)*8);  //npdhash3

}

void ps2_decrypt_image(u8 dex_mode, const u8* klicensee, const char* image_name, const char* data_file)
{
	FILE * in;
	FILE * data_out;
	FILE * meta_out;

	u8 ps2_data_key[0x10];
	u8 ps2_meta_key[0x10];
	u8 iv[0x10];

	int segment_size;
	s64 data_size;
	int i;
	u8 header[256];
	u8 * data_buffer;
	u8 * meta_buffer;
	u32 read = 0;
	int num_child_segments;
	
	int64_t c, flush_size, decr_size, total_size;
	int percent;
	
	decr_size = c = percent = 0;

	//open files
	in = fopen(image_name, "rb");
	meta_out = fopen("/dev_hdd0/tmp/meta_file.bin", "wb");
	data_out = fopen(data_file, "wb");

	//get file info
	read = fread(header, 256, 1, in);
	segment_size = be32(header + 0x84);
	data_size = be64(header + 0x88);
	num_child_segments = segment_size / PS2_META_ENTRY_SIZE;
	
	total_size = data_size;
	flush_size = total_size / 100;

	LOG("segment size: %x\ndata_size: %lx\n", segment_size, data_size);

	//alloc buffers
	data_buffer = malloc(segment_size*num_child_segments);
	meta_buffer = malloc(segment_size);

	//generate keys
	if(dex_mode)
	{
		LOG("[* DEX] PS2 Classic:\n");
		set_ps2_iv(iv);
		aes128cbc_enc(ps2_key_dex_data, iv, klicensee, 0x10, ps2_data_key);
		aes128cbc_enc(ps2_key_dex_meta, iv, klicensee, 0x10, ps2_meta_key);
	}else{
		LOG("[* CEX] PS2 Classic:\n");
		set_ps2_iv(iv);
		aes128cbc_enc(ps2_key_cex_data, iv, klicensee, 0x10, ps2_data_key);
		aes128cbc_enc(ps2_key_cex_meta, iv, klicensee, 0x10, ps2_meta_key);
	}

	//decrypt iso
	fseek(in, segment_size, SEEK_SET);
	
	while(data_size > 0)
	{
		//decrypt meta
		read = fread(meta_buffer, 1, segment_size, in);
		aes128cbc(ps2_meta_key, iv, meta_buffer, read, meta_buffer);
		fwrite(meta_buffer, read, 1, meta_out);

		read = fread(data_buffer, 1, segment_size*num_child_segments, in);		
		for(i=0;i<num_child_segments;i++)	
			aes128cbc(ps2_data_key, iv, data_buffer+(i*segment_size), segment_size, data_buffer+(i*segment_size));
		if(data_size >= read)
			fwrite(data_buffer, read, 1, data_out);
		else
			fwrite(data_buffer, data_size, 1, data_out);
			
		c += read;
		if(c >= flush_size) {
			percent += 1;
			decr_size = decr_size + c;
			LOG("Decrypted: %d Blocks 0x%08lx", percent, decr_size);
			c = 0;
		}
		
		data_size -= read;
	}

	//cleanup
	free(data_buffer);
	free(meta_buffer);

	fclose(in);
	fclose(data_out);
	fclose(meta_out);
}

void ps2_encrypt_image(u8 dex_mode, const char* image_name, const char* data_file)
{
	FILE * in;
	FILE * data_out;

	u8 ps2_data_key[0x10];
	u8 ps2_meta_key[0x10];
	u8 iv[0x10];

	u32 segment_size;
	s64 data_size;
	u32 i;
	u8 * data_buffer;
	u8 * meta_buffer;
	u8 * ps2_header;
	
	u32 read = 0;
	u32 num_child_segments = 0x200;
	u32 segment_number = 0;
	
	int64_t c, flush_size, encr_size, total_size;
	int percent;
	
	encr_size = c = percent = 0;

	//open files
	in = fopen(image_name, "rb");
	data_out = fopen(data_file, "wb");
	
	/* iso9660 check */
	if (!ps2_iso9660_sig(in, image_name))
		return;

	//Get file info
	segment_size = PS2_DEFAULT_SEGMENT_SIZE;
	fseeko(in, 0, SEEK_END);
	data_size = ftello(in);
	fclose(in);
	
	total_size = data_size;
	flush_size = total_size / 100;

	/* limg section */
	ps2_build_limg(image_name, data_size);
	
	// Get new file info -- FIX FAKE SIZE VALUE on PS2 HEADER
	in = fopen(image_name, "rb");
	fseeko(in, 0, SEEK_END);
	data_size = ftello(in);
	fseeko(in, 0, SEEK_SET);

	LOG("segment size: %x\ndata_size: %lx\nfile name: %s\nContent_id: %s\niso %s\nout file: %s\n", segment_size, data_size, PS2_REAL_OUT_NAME, PS2_PLACEHOLDER_CID, image_name, data_file);

	//prepare buffers
	data_buffer = malloc(segment_size * 0x200);
	meta_buffer = malloc(segment_size);
	ps2_header = malloc(segment_size);
	memset(ps2_header, 0, segment_size);

	//generate keys
	if(dex_mode)
	{
		LOG("[* DEX] PS2 Classic:\n");
		set_ps2_iv(iv);
		aes128cbc_enc(ps2_key_dex_data, iv, ps2_placeholder_klic, 0x10, ps2_data_key);
		aes128cbc_enc(ps2_key_dex_meta, iv, ps2_placeholder_klic, 0x10, ps2_meta_key);
	}else{
		LOG("[* CEX] PS2 Classic:\n");
		set_ps2_iv(iv);
		aes128cbc_enc(ps2_key_cex_data, iv, ps2_placeholder_klic, 0x10, ps2_data_key);
		aes128cbc_enc(ps2_key_cex_meta, iv, ps2_placeholder_klic, 0x10, ps2_meta_key);
	}

	//write incomplete ps2 header
	build_ps2_header(ps2_header, 2, PS2_PLACEHOLDER_CID, PS2_REAL_OUT_NAME, data_size);
	fwrite(ps2_header, segment_size, 1, data_out);

	//write encrypted image
	while((read = fread(data_buffer, 1, segment_size*num_child_segments, in)))
	{
		//last segments?
		if(read != (segment_size*num_child_segments)){
			num_child_segments = (read / segment_size);
			if((read % segment_size) > 0)
				num_child_segments++;
		}

		//encrypt data and create meta		
		memset(meta_buffer, 0, segment_size);
		for(i=0;i<num_child_segments;i++)
		{	
			aes128cbc_enc(ps2_data_key, iv, data_buffer+(i*segment_size), segment_size, data_buffer+(i*segment_size));
			sha1(data_buffer+(i*segment_size), segment_size, meta_buffer+(i*PS2_META_ENTRY_SIZE));
			wbe32(meta_buffer+(i*PS2_META_ENTRY_SIZE)+0x14, segment_number);
			segment_number++;
		}

		//encrypt meta
		aes128cbc_enc(ps2_meta_key, iv, meta_buffer, segment_size, meta_buffer);
		
		//write meta and data
		fwrite(meta_buffer, segment_size, 1, data_out);
		fwrite(data_buffer, segment_size, num_child_segments, data_out);
		
		c += read;
		if(c >= flush_size) {
			percent += 1;
			encr_size = encr_size + c;
			LOG("Encrypted: %d Blocks 0x%08lx", percent, encr_size);
			c = 0;
		}
	
		memset(data_buffer, 0, segment_size*num_child_segments);
	}

	//finalize ps2_header
	// - wtf is between signature and first segment?

	//cleanup
	free(data_buffer);
	free(meta_buffer);
	free(ps2_header);

	fclose(in);
	fclose(data_out);
}

void ps2_crypt_vmc(u8 dex_mode, const char* vmc_path, const char* vmc_out, int crypt_mode)
{
	FILE * in;
	FILE * data_out;

	u8 ps2_vmc_key[0x10];
	u8 iv[0x10];

	int segment_size;
	u8 * data_buffer;
	u32 read = 0;

	segment_size = PS2_DEFAULT_SEGMENT_SIZE;
	
	// Validate new hashes
	if ((crypt_mode == PS2_VMC_ENCRYPT) && !vmc_hash(vmc_path))
		return;

	//open files
	in = fopen(vmc_path, "rb");
	data_out = fopen(vmc_out, "wb");

	//alloc buffers
	data_buffer = malloc(segment_size);

	//generate keys
	if(dex_mode)
	{
        LOG("VMC (dex)\n");
		memcpy(ps2_vmc_key, ps2_key_dex_vmc, 0x10);
	}else{
        LOG("VMC (cex)\n");
		memcpy(ps2_vmc_key, ps2_key_cex_vmc, 0x10);
	}
	
	set_ps2_iv(iv);

	while((read = fread(data_buffer, 1, segment_size, in)))
	{
		//decrypt or encrypt vmc
		if(crypt_mode == PS2_VMC_DECRYPT)
			aes128cbc(ps2_vmc_key, iv, data_buffer, read, data_buffer);
		else
		{
            aes128cbc_enc(ps2_vmc_key, iv, data_buffer, read, data_buffer);
        }
		fwrite(data_buffer, read, 1, data_out);

	}

	//cleanup
	free(data_buffer);

	fclose(in);
	fclose(data_out);
	
}

/*
int ps2classics_main(int argc, char *argv[])
{
	{
		LOG("usage:\n\tiso:\n\t\t%s d [cex/dex] [klicensee] [encrypted image] [out data] [out meta]\n", argv[0]);
		LOG("\t\t%s e [cex/dex] [klicensee] [iso] [out data] [real out name] [CID]\n", argv[0]);
		LOG("\tvmc:\n\t\t%s vd [cex/dex] [vme file] [out vmc]\n", argv[0]);
		LOG("\t\t%s ve [cex/dex] [vmc file] [out vme]\n", argv[0]);
		exit(0);
	}
}
*/