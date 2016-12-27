/*********************************************************************************************************************

                                                      Buffer.cpp

						                    Copyright 2002, John J. Bolton
	--------------------------------------------------------------------------------------------------------------

	$Header: //depot/Libraries/Buffer/Buffer.cpp#5 $

	$NoKeywords: $

*********************************************************************************************************************/

#include "Buffer.h"

#include "Misc/exceptions.h"
#include "Misc/assert.h"
#include "Misc/max.h"

#include <cstring>

namespace
{
	inline bool _IsPowerOf2( int n )
	{
		return ( ( n & ( n - 1 ) ) == 0 );
	}

	inline bool _IsMultipleOf( int n, int m )
	{
		return ( n % m == 0 );
	}

	inline bool	_IsAligned( unsigned n, unsigned align )
	{
		assert_power_of_two( align + 1 );
		return ( ( n & align ) == 0 );
	}

	inline int _HighestMultiple( int n, int m )
	{
		return ( n - n % m );
	}

	inline int _HighestMultiplePowerOf2( int n, int align )
	{
		assert_power_of_two( align + 1 );
		return ( n & ~align );
	}

	inline int _Pad( int n, int m )
	{
		return _HighestMultiple( n, m ) + m;
	}

	inline unsigned _PadPowerOf2( unsigned n, unsigned align )
	{
		assert_power_of_two( align + 1 );
		return ( ( n + align ) & ~align );
	}

} // anonymous namespace


/********************************************************************************************************************/
/*																													*/
/********************************************************************************************************************/

//! @param	pBuffer				Memory for use by the buffer. The address must be aligned on a @a bufferAlign
//!								boundary.
//! @param	bufferSize			Size of the buffer. It must be a multiple of @a blockSize.
//! @param	handle				Handle to be passed to the buffered object.
//! @param	pBufferedObject		Interface to the object that fills and flushes the buffer.
//! @param	flags				Configuration flags (see enum Flags)
//! @param	blockSize			The buffered object will always be asked to fill or flush a multiple of this size.
//! @param	sectorAlign			Locations in the buffered object are always aligned on this boundary. This value
//!								must be a power of two. If the sector alignment is higher than the block size, then
//!								the sector alignment must be a multiple of the block size. If the block size is
//!								higher than sector alignment, then the opposite must be true.
//! @param	bufferAlign			The buffered object is always asked to fill or flush starting at an memory address
//!								aligned on this boundary. This value must be a power of two.

BufferedProxy::BufferedProxy( void * pBuffer,
			  int bufferSize,
			  unsigned handle,
			  BufferedObject * pBufferedObject,
			  unsigned flags,
			  int blockSize				/* = 1*/,
			  unsigned sectorAlign		/* = 1*/,
			  unsigned bufferAlign		/* = 1*/ )
{
	// The block alignment must be a power of two

	if ( !_IsPowerOf2( sectorAlign )  )
	{
		throw ConstructorFailedException( "The sector alignment must be a power of two." );
	}

	// The buffer alignment must be a power of two

	if ( !_IsPowerOf2( bufferAlign ) )
	{
		throw ConstructorFailedException( "The buffer alignment must be a power of two." );
	}

	// The buffer size must be a multiple of the block size

	if ( bufferSize % blockSize != 0 )
	{
		throw ConstructorFailedException( "The buffer size must be a multiple of the block size." );
	}

	// The buffer must be aligned on a bufferAlign boundary

	if ( !_IsAligned( reinterpret_cast< unsigned long >( pBuffer ), bufferAlign-1 ) )
	{
		throw ConstructorFailedException( "The buffer must be aligned on a bufferAlign boundary." );
	}

	// If the sector alignment is bigger than the block size, then the sector alignment must be a
	// multiple of the block size. If the block size is bigger than sector alignment, then the opposite
	// must be true.
	//

	if ( static_cast< int >( sectorAlign ) > blockSize )
	{
		if ( !_IsMultipleOf( sectorAlign, blockSize ) )
		{
			throw ConstructorFailedException( "The sector alignement must be a multiple of the block size." );
		}
	}
	else
	{
		if ( !_IsMultipleOf( blockSize, sectorAlign ) )
		{
			throw ConstructorFailedException( "The block size must be a multiple of the sector alignement." );
		}
	}

	m_Handle				= handle;
	m_paBuffer				= (char *)pBuffer;
	m_BufferSize			= bufferSize;
	m_BufferSizeInBlocks	= bufferSize / blockSize;
	m_pBufferedObject		= pBufferedObject;
	m_Flags					= flags;
	m_BlockSize				= blockSize;
	m_SectorAlign			= sectorAlign - 1;		// Store the mask
	m_BufferAlign			= bufferAlign - 1;		// Store the mask
	m_Point					= 0;
	m_BufferLoc				= 0;
	m_DataSize				= 0;
	m_IsDirty				= false;
}



/********************************************************************************************************************/
/*																													*/
/********************************************************************************************************************/

BufferedProxy::~BufferedProxy()
{
	Flush();
}


/********************************************************************************************************************/
/*																													*/
/********************************************************************************************************************/

//! @param	pDst	Location to which data is to be copied from the buffer
//! @param	n		Number of bytes to read
//!
//! @return		The number of bytes actually read.

int BufferedProxy::Read( void * pDst, int n )
{
	int	bytesToRead;
	int	totalRead			= 0;

	// First, read what is already available in the buffer (if any)

	bytesToRead = std::min( n, RemainingReadAmount() );
	if ( bytesToRead > 0 )
	{
		CopyOut( &pDst, bytesToRead );
		totalRead += bytesToRead;
		n -= bytesToRead;
	}

	// At this point, the read is done or it has reached the end of the buffer.
	//
	// Next, if the remaining amount to read is greater than or equal to the buffer size, then read as many
	// buffer-sized blocks as possible.  

	if ( n >= m_BufferSize )
	{
		// About to do fills, so a flush is needed.

		Flush();

		// If the CF_NO_DIRECT_IO flag is not set and the destination buffer is aligned, read the data directly
		// into the destination buffer.

		if (    ( m_Flags & CF_NO_DIRECT_IO ) == 0
			 && _IsAligned( reinterpret_cast< unsigned long >( pDst ), m_BufferAlign ) )
		{
			m_BufferLoc += m_DataSize;							// Move the buffer to the next data to read in the buffered object

			int const	blocksToRead = _HighestMultiple( n, m_BufferSize ) / m_BlockSize;	// Read a multiple of the buffer size

			int const	blocksRead	= m_pBufferedObject->Read( m_Handle,
															   reinterpret_cast< char * >( pDst ),
															   blocksToRead );

			int const	bytesRead	= blocksRead * m_BlockSize;

			pDst = reinterpret_cast< char * >( pDst ) + bytesRead;
			totalRead += bytesRead;
			n -= bytesRead;

			// The buffer must be resynched.

			m_BufferLoc += blocksRead;
			m_Point = 0;
			m_DataSize = 0;
		}

		// Otherwise, read the data a buffer at time until less than a full buffer is needed or until the end of
		// the data is reached.

		else
		{
			while ( n >= m_BufferSize )
			{
				// Load the buffer

				m_BufferLoc += m_DataSize;		// Move the buffer location to the data to be filled
				Fill();

				// Copy a full buffer

				CopyOut( &pDst, m_DataSize * m_BlockSize );
				totalRead += m_DataSize * m_BlockSize;
				n -= m_DataSize * m_BlockSize;

				// If the end of the data was reached, then abort

				if ( m_DataSize < m_BufferSize )
				{
					break;
				}
			}
		}
	}

	// At this point, the read is done or it has reached the end of the buffer.
	//
	// Read the rest of the data through the buffer.

	if ( n > 0 )
	{
		// About to do a fill, so a flush is needed.

		Flush();

		m_BufferLoc += m_DataSize;	// Bump the location of the buffer
		Fill();

		bytesToRead = std::min( n, RemainingReadAmount() );
		if ( bytesToRead > 0 )
		{
			CopyOut( &pDst, bytesToRead );
			totalRead += bytesToRead;
			n -= bytesToRead;
		}

	}

	return totalRead;
}



/********************************************************************************************************************/
/*																													*/
/********************************************************************************************************************/

//! @param	pSrc	Location from which data is to be copied to the buffer.
//! @param	n		Number of bytes to copy
//!
//! @return		The actual number of bytes written

int BufferedProxy::Write( void const * pSrc, int n )
{
	int			totalWritten	= 0;

	// First, write to the remaining space available in the buffer (if any)

	{
		int	const	bytesToWrite	= std::min( n, RemainingWriteSpace() );

		if ( bytesToWrite > 0 )
		{
			CopyIn( &pSrc, bytesToWrite );
			totalWritten += bytesToWrite;
			n -= bytesToWrite;
		}
	}

	// At this point the write is done or the buffer is full.
	//
	// Next, if the remaining amount to write is greater than or equal to the buffer size, then write the largest
	// multiple of the buffer size as possible.  

	if ( n >= m_BufferSize )
	{
		// The buffer is full, so it must be flushed first.

		Flush();
		m_BufferLoc += m_DataSize;

		// If the CF_NO_DIRECT_IO flag is not set and the source buffer is aligned, write the data directly
		// from the source buffer.

		if ( ( m_Flags & CF_NO_DIRECT_IO ) == 0 &&
			 _IsAligned( reinterpret_cast< unsigned long >( pSrc ), m_BufferAlign ) )
		{
			int const	blocksToWrite	= _HighestMultiple( n, m_BufferSize ) / m_BlockSize;	// Write a multiple of the buffer size

			int const	blocksWritten	= m_pBufferedObject->Write( m_Handle,
																	reinterpret_cast< char const * >( pSrc ),
																	blocksToWrite );

			int const	bytesWritten	= blocksWritten * m_BlockSize;

			pSrc = reinterpret_cast< char const * >( pSrc ) + bytesWritten;
			totalWritten += bytesWritten;
			n -= bytesWritten;

			// The buffer must be resynched.

			m_BufferLoc += blocksWritten;
			m_Point = 0;
			m_DataSize = 0;
		}

		// Otherwise, write the data a buffer at time until less than a full buffer is left to write.

		else
		{
			while ( n >= m_BufferSize )
			{
				// Copy a full buffer

				CopyIn( &pSrc, m_BufferSize );
				totalWritten += m_BufferSize;
				n -= m_BufferSize;

				// Flush the buffer

				Flush();
				m_BufferLoc += m_BufferSizeInBlocks;		// Move the buffer location to the data to be filled
			}
		}
	}


	// Write the rest through the buffer
	//
	// At this point the buffer is empty and needs to be filled.

	while ( n > 0 )
	{
		// The buffer is full, so it must be flushed.

		Flush();

		// If we are 
		if ( n < m_BufferSize && ( m_Flags & CF_NO_FILLS ) == 0 )
		{
			Fill();
		}

		int const bytesToWrite = std::min( n, RemainingWriteSpace() );
		if ( bytesToWrite <= 0 )
		{
			break;	// Reached the end of the data
		}

		CopyIn( &pSrc, bytesToWrite );
		totalWritten += bytesToWrite;
		n -= bytesToWrite;

		if ( m_Point >= m_BufferSize )
		{
			Flush();
		}
	}

	return totalWritten;

};


/********************************************************************************************************************/
/*																													*/
/********************************************************************************************************************/

//!
//! @param	location	Where to put the current location (specified as the number of bytes from the beginning).

int BufferedProxy::Seek( int location )
{
	// If the seek location is already in the buffer, then just move the index

	if ( location >= m_BufferLoc * m_BlockSize && location < ( m_BufferLoc + m_DataSize ) * m_BlockSize )
	{
		m_Point = location - m_BufferLoc * m_BlockSize;
	}
	else
	{
		// Flush the buffer before seeking

		Flush();

		// Seek to the new location
		//
		// Note that the result of the seek is not guaranteed to be the intended location. This can happen if the
		// intended location is before the start of the data or after the end.

		int const	alignedLocation	= _HighestMultiplePowerOf2( location, m_SectorAlign );	// The start of the buffer must be aligned
		m_BufferLoc	= m_pBufferedObject->Seek( m_Handle, alignedLocation / m_BlockSize );

		// Fill the buffer

		Fill();

		// Point to the seek location in the buffer
		//
		// Note that if the intended location is outside the range of the data in the buffer, a reasonable location is
		// chosen instead. This can happen if the intended location is before the start of the data or after the end.

		if ( location < m_BufferLoc * m_BlockSize || location >= ( m_BufferLoc + m_DataSize ) * m_BlockSize )
		{
		}

		m_Point += location - m_BufferLoc * m_BlockSize;			// ...and the seek location may be in the middle
	}

	return location;
};



/********************************************************************************************************************/
/*																													*/
/********************************************************************************************************************/

void BufferedProxy::Flush()
{
	if ( m_IsDirty && m_DataSize > 0 )
	{
		int		seekValue;

		assert( _IsAligned( m_BufferLoc * m_BlockSize, m_SectorAlign ) );

		// Seek to the proper location and send the data to the buffered object. Reset the dirty flag if all the
		// data was written.

		seekValue = m_pBufferedObject->Seek( m_Handle, m_BufferLoc );
		if ( seekValue == m_BufferLoc )
		{
			int const	blocksToFlush	= m_DataSize;

			int const	blocksFlushed	= m_pBufferedObject->Write( m_Handle, m_paBuffer, blocksToFlush );

			if ( blocksFlushed == m_DataSize )
			{
				m_IsDirty = false;
			}

		}
	}
}


/********************************************************************************************************************/
/*																													*/
/********************************************************************************************************************/

//!
//! @warning	Any data in the buffer that has not been flushed will be overwritten.

void BufferedProxy::Fill()
{
	if ( ( m_Flags & CF_NO_FILLS ) == 0 )
	{
		int		seekValue;

		// Seek to the proper location and read the data from the buffered object.

		seekValue = m_pBufferedObject->Seek( m_Handle, m_BufferLoc );
		if ( seekValue == m_BufferLoc )
		{
			m_DataSize = m_pBufferedObject->Read( m_Handle, m_paBuffer, m_BufferSizeInBlocks );
		}
	}

	m_Point = 0;
}


/********************************************************************************************************************/
/*																													*/
/********************************************************************************************************************/

int BufferedProxy::RemainingReadAmount() const
{
	return m_DataSize * m_BlockSize - m_Point;
}


/********************************************************************************************************************/
/*																													*/
/********************************************************************************************************************/

int BufferedProxy::RemainingWriteSpace() const
{
	return m_BufferSizeInBlocks * m_BlockSize - m_Point;
}


/********************************************************************************************************************/
/*																													*/
/********************************************************************************************************************/

void BufferedProxy::CopyOut( void * * ppDst, int n )
{
	assert( m_Point + n <= m_DataSize * m_BlockSize );

	void * const	pDst	= *ppDst;	// Convenience

	// Copy from the buffer
	memcpy( pDst, &m_paBuffer[ m_Point ], n );

	// Bump the pointers

	m_Point += n;
	*ppDst = reinterpret_cast< char * >( pDst ) + n;
}


/********************************************************************************************************************/
/*																													*/
/********************************************************************************************************************/

void BufferedProxy::CopyIn( void const * * ppSrc, int n )
{
	assert( m_Point + n <= m_BufferSize );

	void const * const	pSrc	= *ppSrc;	// Convenience

	// Copy to the buffer

	memcpy( &m_paBuffer[ m_Point ], pSrc, n );

	// Bump pointers

	m_Point += n;
	*ppSrc = reinterpret_cast< char const * >( pSrc ) + n;

	// Mark the buffer as dirty

	m_IsDirty = true;

	// If the size of the data in the buffer is growing, then update the size.

	if ( m_Point > m_DataSize * m_BlockSize )
	{
		m_DataSize = ( m_Point + m_BlockSize-1 ) / m_BlockSize;
	}
}

