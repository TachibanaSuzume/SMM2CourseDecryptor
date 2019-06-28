#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "aes.h"
#include "keys.h"

#define STATE_SIZE 4
#define NUM_ROUNDS 4

#define SWAP(x) ((x >> 24) & 0xff | (x >> 8) & 0xff00 | (x << 8) & 0xff0000 | (x << 24) & 0xff000000)

void rand_init(uint32_t *rand_state, uint32_t in1, uint32_t in2, uint32_t in3, uint32_t in4)
{
	int cond = in1 | in2 | in3 | in4;
	
	rand_state[0] = cond ? in1 : 1;
	rand_state[1] = cond ? in2 : 0x6C078967;	
	rand_state[2] = cond ? in3 : 0x714ACB41;
	rand_state[3] = cond ? in4 : 0x48077044;
}

uint32_t rand_gen(uint32_t *rand_state)
{	
	uint32_t n = rand_state[0] ^ rand_state[0] << 11;
	rand_state[0] = rand_state[1];
	n ^= n >> 8 ^ rand_state[3] ^ rand_state[3] >> 19;
	rand_state[1] = rand_state[2];
	rand_state[2] = rand_state[3];
	rand_state[3] = n;
	return n;
}

void gen_key(uint32_t *key_table, uint32_t *out_key, uint32_t *rand_state)
{
	out_key[0] = 0;
	
	for (int i = 0; i < STATE_SIZE; i++)
	{		
		for (int j = 0; j < NUM_ROUNDS; j++)
		{
			out_key[i] <<= 8;
			out_key[i] |= (key_table[rand_gen(rand_state) >> 26] >> ((rand_gen(rand_state) >> 27) & 24)) & 0xFF;
		}
	}
}

int main(int argc, char **argv)
{
	if (argc != 3)
	{
		printf("Usage: %s input output\n", argv[0]);
		return 0;
	}
	
	FILE *in = fopen(argv[1], "rb");
	FILE *out = fopen(argv[2], "wb");
	
	fseek(in, 0, SEEK_END);
	size_t sz = ftell(in);
	rewind(in);
	
	struct AES_ctx ctx;
	
	uint32_t rand_state[STATE_SIZE];
	uint32_t key_state[STATE_SIZE];
	
	switch(sz) // CMAC calculation isn't handled yet, will implement eventually
	{
		case 0x1C000: // Thumbnail image data
		{
			printf("Decrypting thumbnail %s to %s...\n", argv[1], argv[2]);
			
			char *buf = malloc(0x1C000);
			
			fread(buf, 1, 0x1C000, in);
			fclose(in);
			
			char *end = buf + sz - 0x30;
			
			rand_init(rand_state, *(uint32_t *)&end[0x10], *(uint32_t *)&end[0x14], *(uint32_t *)&end[0x18], *(uint32_t *)&end[0x1C]);
			gen_key(thumb_key_table, key_state, rand_state);
			
			AES_init_ctx_iv(&ctx, (uint8_t *)key_state, end);		
			AES_CBC_decrypt_buffer(&ctx, buf, sz - 0x30);
			
			fwrite(buf, 1, sz - 0x30, out);
			fclose(out);
			
			free(buf);
			
			break;
		}
		
		case 0x5C000: // Course data
		{
			printf("Decrypting course %s to %s...\n", argv[1], argv[2]);

			char *buf = malloc(0x5C000);
			
			fread(buf, 1, 0x5C000, in);
			fclose(in);
			
			char *end = buf + sz - 0x30;
			
			rand_init(rand_state, *(uint32_t *)&end[0x10], *(uint32_t *)&end[0x14], *(uint32_t *)&end[0x18], *(uint32_t *)&end[0x1C]);
			gen_key(course_key_table, key_state, rand_state);
			
			AES_init_ctx_iv(&ctx, (uint8_t *)key_state, end);		
			AES_CBC_decrypt_buffer(&ctx, buf + 0x10, sz - 0x40);
			
			fwrite(buf + 0x10, 1, sz - 0x40, out);
			fclose(out);

			free(buf);
			
			break;
		}
		
		case 0x68000: // Replay data
		{
			printf("Decrypting replay %s to %s...\n", argv[1], argv[2]);

			char *buf = malloc(0x68000);
			
			fread(buf, 1, 0x68000, in);
			fclose(in);
			
			char *end = buf + sz - 0x30;
			
			rand_init(rand_state, *(uint32_t *)&end[0x10], *(uint32_t *)&end[0x14], *(uint32_t *)&end[0x18], *(uint32_t *)&end[0x1C]);
			gen_key(replay_key_table, key_state, rand_state);
			
			AES_init_ctx_iv(&ctx, (uint8_t *)key_state, end);		
			AES_CBC_decrypt_buffer(&ctx, buf, sz - 0x30);
			
			fwrite(buf, 1, sz - 0x30, out);
			fclose(out);

			free(buf);
			
			break;
		}
		
		default:
		{
			puts("Error: Unsupported file!");
			fclose(in);
			fclose(out);
			return 0;
		}
	}
	
	puts("Done!");
	
	return 0;
}