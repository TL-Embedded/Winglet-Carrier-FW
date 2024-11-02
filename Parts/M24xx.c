
#include "M24xx.h"
#include "I2C.h"
#include "Core.h"

/*
 * PRIVATE DEFINITIONS
 */

#define M24XX_ADDR					0x50
#define M24XX_TIMEOUT				20

#if (M24XX_SERIES == 0)				// 16 B
#define M24XX_ADDR_BITS				4
#define M24_PAGE_SIZE				16
#elif (M24XX_SERIES == 1)			// 128 B
#define M24XX_ADDR_BITS				7
#define M24_PAGE_SIZE				8
#elif (M24XX_SERIES == 2)			// 256 B
#define M24XX_ADDR_BITS				8
#define M24_PAGE_SIZE				8
#elif (M24XX_SERIES == 4)			// 512 B
#define M24XX_ADDR_BITS				9
#define M24_PAGE_SIZE				16
#elif (M24XX_SERIES == 8)			// 1 kB
#define M24XX_ADDR_BITS				10
#define M24XX_PAGE_SIZE				16
#elif (M24XX_SERIES == 16)			// 2 kB
#define M24XX_ADDR_BITS				11
#define M24XX_PAGE_SIZE				16
#elif (M24XX_SERIES == 32)			// 4 kB
#define M24XX_ADDR_BITS				12
#define M24XX_PAGE_SIZE				32
#elif (M24XX_SERIES == 64)			// 8 kB
#define M24XX_ADDR_BITS				13
#define M24XX_PAGE_SIZE				32
#elif (M24XX_SERIES == 128)			// 16 kB
#define M24XX_ADDR_BITS				14
#define M24XX_PAGE_SIZE				64
#elif (M24XX_SERIES == 256)			// 32 kB
#define M24XX_ADDR_BITS				15
#define M24XX_PAGE_SIZE				64
#elif (M24XX_SERIES == 512)			// 64 kB
#define M24XX_ADDR_BITS				16
#define M24XX_PAGE_SIZE				128
#elif (M24XX_SERIES == 1024) 		// 128 kB
#define M24XX_ADDR_BITS				17
#define M24XX_PAGE_SIZE				128
#else
#error "Unknown 24xx series"
#endif

#if (M24XX_SERIES <= 16)
#define M24XX_ADDR_SIZE				1
#else
#define M24XX_ADDR_SIZE				2
#endif

#if (M24XX_SERIES == 1024)
// For this part, the address order is inconvenient - the block address is the high bit.
#define M24XX_DEV_ADDR(_addr) 		(M24XX_ADDR | (((_addr) >> 14) & 0x04) | (((_addr) >> 17) & 0x03))
#elif (M24XX_ADDR_BITS > M24XX_ADDR_SIZE * 8)
// For these devices, there is more addressable memory than the address byte allows.
// Some or all of the A[2:0] bits are used as block addresses
#define M24XX_DEV_ADDR(_addr)		(M24XX_ADDR | ((_addr >> (M24XX_ADDR_SIZE * 8)) & 0x7))
#else
// The address bits A[2:0] are used to select chained devices
#define M24XX_DEV_ADDR(_addr)		(M24XX_ADDR | ((_addr >> M24XX_ADDR_BITS) & 0x7))
#endif


/*
 * PRIVATE TYPES
 */

/*
 * PRIVATE PROTOTYPES
 */

static bool M24xx_WaitForIdle(uint8_t device);

/*
 * PRIVATE VARIABLES
 */

/*
 * PUBLIC FUNCTIONS
 */

bool M24xx_Init(void)
{
	return M24xx_WaitForIdle( M24XX_DEV_ADDR(0) );
}

bool M24xx_Write(uint32_t pos, const uint8_t * bfr, uint32_t size)
{
	uint32_t end = pos + size;
	while (pos < end)
	{
		// Select the next chunk, and guarantee page alignment.
		uint32_t page_end = (pos + M24_PAGE_SIZE) & ~(M24_PAGE_SIZE-1);
		uint32_t length = (page_end < end ? page_end : end) - pos;

		// Start address followed by data
		uint8_t tx[M24_PAGE_SIZE + M24XX_ADDR_SIZE];
#if (M24XX_ADDR_SIZE == 2)
		tx[0] = (uint8_t)(pos >> 8);
		tx[1] = (uint8_t)pos;
#else
		tx[0] = (uint8_t)pos;
#endif
		memcpy(tx + M24XX_ADDR_SIZE, bfr, length);

		uint8_t device = M24XX_DEV_ADDR(pos);
		if (!(M24xx_WaitForIdle(device) && I2C_Write(M24XX_I2C, device, tx, length + M24XX_ADDR_SIZE)))
		{
			return false;
		}

		bfr += length;
		pos += length;
	}
	return true;
}

bool M24xx_Read(uint32_t pos, uint8_t * bfr, uint32_t size)
{
	uint8_t device = M24XX_DEV_ADDR(pos);
	if (!M24xx_WaitForIdle(device)) { return false; }

	// A read starts by writing the destination address
	uint8_t tx[M24XX_ADDR_SIZE] = {
#if (M24XX_ADDR_SIZE == 2)
		(uint8_t)(pos >> 8),
#endif
		(uint8_t)pos
	};

	return I2C_Transfer(M24XX_I2C, device,
			tx, sizeof(tx), // Write buffer
			bfr, size       // Read buffer
			);
}

/*
 * PRIVATE FUNCTIONS
 */

static bool M24xx_WaitForIdle(uint8_t device)
{
	uint32_t start = CORE_GetTick();
	while (CORE_GetTick() - start < M24XX_TIMEOUT)
	{
		// IC will not ack if busy
		if (I2C_Scan(M24XX_I2C, device))
		{
			return true;
		}
		CORE_Idle();
	}
	return false;
}

/*
 * INTERRUPT ROUTINES
 */

