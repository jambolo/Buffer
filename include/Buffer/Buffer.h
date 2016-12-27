#if !defined( BUFFER_H_INCLUDED )
#define BUFFER_H_INCLUDED

#pragma once

/** @file *//********************************************************************************************************

                                                       Buffer.h

						                    Copyright 2003, John J. Bolton
	--------------------------------------------------------------------------------------------------------------

	$Header: //depot/Libraries/Buffer/Buffer.h#3 $

	$NoKeywords: $

*********************************************************************************************************************/

//! A stream buffer that enables non-aligned and non-blocksize I/O to/from an object that requires aligned and/or
//! block I/O or requires I/O to/from a specific memory location.

class BufferedProxy
{
public:

	//! Configuration flags
	enum
	{
		//! The buffer normally assumes that both reading and writing will be performed. Performance can be
		//! improved when I/O is one or the other, but not both.

		CF_READ_ONLY		= 0x00000001,	//!< Allow only reads
		CF_WRITE_ONLY		= 0x00000002,	//!< Allow only writes

		//! In order to improve performance, reads and writes that are larger than the size of the buffer normally
		//! bypass the buffer (as long as the data can be properly aligned). Sometimes, this is not desirable
		//! (e.g. when the buffered object can only access a restricted address space). This flag means always do
		//! reads and writes indirectly through the buffer.

		CF_NO_DIRECT_IO		= 0x00000004,	//!< Never bypass the buffer.

		//! In order to support read and update, the buffer is normally filled with data from the buffered object
		//! whenever a location is accessed that is not already in the buffer. This behavior is unnecessary in many
		//! cases. This flag improves performance by indicating the buffer should never be filled from the buffered
		//! object.

		CF_NO_FILLS			= 0x00000008,	//!< Never fill the buffer from the buffered object.

		//! The buffer normally assumes that most I/O is sequential or reads are larger than the size of a buffer.
		//! If that is not the case, the buffer may perform many unnecessary fills. This flag improves performance
		//! when the I/O is mostly random and the size of the reads are usually smaller than the size of the
		//! buffer.

		CF_RANDOM_ACCESS	= 0x00000010,	//!< Assume mostly random access
	};

	//! Buffered object
	//
	//!
	//! Derive from this class to implement functions required by a Buffer. The Buffer reads and writes data to/from
	//! the buffered object through this interface.

	class BufferedObject
	{
	public:

		//! Reads @a n bytes of data from @a handle to the buffer starting at @a pBuffer. Returns the number of blocks read.
		//
		//! @param	handle	Handle provided to the Buffer.
		//! @param	pBuffer	Location to put the data.
		//! @param	n		Number of blocks to read.
		//!
		//! @return		Number of bytes actually read -- must be a multiple of the block size
		//!
		//! @note	@a handle's "current location" is expected to move to the next byte following the data that
		//!			was copied to the buffer.
		//! @note	@a n is always a multiple of the block size.
		//! @note	@a pBuffer is always aligned to the buffer alignment.

		virtual int	Read( unsigned handle, char * pBuffer, int n )			= 0;

		//! Writes @a n bytes of data from the buffer starting at @a pBuffer to @a handle. Returns the number of blocks written.
		//
		//! @param	handle	Handle provided to the Buffer.
		//! @param	pBuffer	Location to put the data.
		//! @param	n		Number of blocks to write.
		//!
		//! @return		Number of bytes actually written -- must be a multiple of the block size
		//!
		//! @note	@a handle's "current location" is expected to move to the next byte following the data that
		//!			was copied from the buffer.
		//! @note	@a pBuffer is always aligned to the buffer alignment.

		virtual int	Write( unsigned handle, char const * pBuffer, int n )	= 0;

		//! Sets @a handle's "current location" to the given location. Returns the resulting location.
		//
		//! @param	handle		Handle provided to the Buffer.
		//! @param	location	Where to put the "current location" (which block).
		//!
		//! @return		Resulting block location, or < 0 if there was an error.
		//!
		//! @note	The data in the buffered object is assumed to start at 0.
		
		virtual int	Seek( unsigned handle, int location )					= 0;
	};

	//! Constructor
	BufferedProxy( void * pBuffer,				
		   int bufferSize,				
		   unsigned handle,				
		   BufferedObject * pBufferedObject,
		   unsigned flags,				
		   int blockSize		= 1,	
		   unsigned sectorAlign	= 1,	
		   unsigned bufferAlign	= 1		
										
		 );
	virtual ~BufferedProxy();

	//! Returns the number of bytes that can be written before the buffer will have to be flushed.
	int RemainingWriteSpace() const;

	//! Returns the number of bytes that can be read before the buffer will have to be filled.
	int RemainingReadAmount() const;

	//! Reads @a n bytes from the buffered object through the buffer. Returns the number of bytes read, or < 0 if there is an error.
	int Read( void * pDst, int n );

	//! Writes @a n bytes to the buffered object through the buffer. Returns the number of bytes written, or < 0 if there is an error.
	int Write( void const * pSrc, int n );

	//! Moves the current location in the buffered object. Returns the actual location, or < 0 if there is an error.
	int Seek( int location );

	//! Forces the buffer to flush any unwritten data to the buffered object.
	void Flush();

	//! Forces the buffer to refresh itself from the buffered object.
	void Fill();

private:

	// Copy data from the source into the buffer and update the pointers.
	void CopyIn( void const ** ppSrc, int n );

	// Copy data from the buffer to the destination and update the pointers.
	void CopyOut( void ** ppDst, int n );

	unsigned			m_Handle;				// Handle to pass to callback functions
	char *				m_paBuffer;				// Address of the buffer's buffer
	int					m_BufferSize;			// Size of the buffer (in bytes)
	int					m_BufferSizeInBlocks;	// Size of the buffer (in blocks)
	BufferedObject *	m_pBufferedObject;		// The interface to the buffered object
	int					m_BlockSize;			// All fills and flushes are a multiple of this size
	unsigned			m_SectorAlign;			// Fills and flushes start on this boundary on the buffered object
	unsigned			m_BufferAlign;			// Fills and flushes start on this boundary in memory
	unsigned			m_Flags;				// Flags
	int					m_Point;				// Index of the I/O point in the buffer (in bytes)
	int					m_BufferLoc;			// Location of the buffer in the buffered object's space (in blocks)
	int					m_DataSize;				// Size of data in the buffer in bytes (sometimes the buffer is not full)
	bool				m_IsDirty;				// True if the buffer contains data that has not been flushed yet
};


#endif // !defined( BUFFER_H_INCLUDED )
