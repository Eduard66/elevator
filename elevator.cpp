// elevator.cpp : Defines the entry point for the console application.
//

#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <chrono>
#include <regex>
#include <queue>

#define EL_TURNOFF		0
#define	EL_TOLEVEL		1
#define	EL_ATLEVEL		2

typedef struct ElMessage
{
	int		Code,
			Value;

	ElMessage(int code, int value)
	{
		Code		= code;
		Value		= value;
	};

} ELMESSAGE;

std::queue<ELMESSAGE>		qMessages;

bool
commandToLevel(std::string in, std::mutex	&mutexCout, std::mutex	&mutexQueue, int numLevels)
{
	std::regex mask{ "to\\d{1,2}" };
	if (std::regex_match(in, mask)) 
	{
		int	iLevel = std::stoi(in.substr(2)) - 1;
		if ((iLevel + 1) > numLevels)
		{
			mutexCout.lock();
			std::cout << "There is onle " << numLevels << " levels" << std::endl;
			mutexCout.unlock();
			return false;
		}
		mutexQueue.lock( );
		qMessages.push( ELMESSAGE( EL_TOLEVEL, iLevel ) );
		mutexQueue.unlock( );
		return true;
	}
	return false;
}

bool
commandAtLevel(std::string in, std::mutex	&mutexCout, std::mutex	&mutexQueue, int numLevels)
{
	std::regex	mask{ "at\\d{1,2}" };
	if (std::regex_match(in, mask)) {
		int	iLevel = std::stoi(in.substr(2)) - 1;
		if ((iLevel + 1) > numLevels)
		{
			mutexCout.lock();
			std::cout << "There is only " << numLevels << " levels" << std::endl;
			mutexCout.unlock();
			return false;
		}
		mutexQueue.lock( );
		qMessages.push( ELMESSAGE( EL_ATLEVEL, iLevel ) );
		mutexQueue.unlock( );
		return true;
	}
	return false;
}

bool
commandExit(std::string in)
{
	std::regex	mask{ "exit" };
	if (std::regex_match(in, mask))
	{
		qMessages.push( ELMESSAGE( EL_TURNOFF, 0 ) );
		return true;
	}
	return false;
}

void
getElevatorParams(char *argv[], int &numLevels, double &elSpeed, double &levelHeight, int &openTime)
{
	if ((numLevels = std::stoi(argv[1])) == 0)
		throw 1;

	if ((levelHeight = std::stod(argv[2])) == 0)
		throw 2;

	if ((elSpeed = std::stod(argv[3])) == 0)
		throw 3;

	if ((openTime = std::stoi(argv[4])) == 0)
		throw 4;
}

class CElevator
{
	int						m_ElevatorCurrentPos,
							m_MovementPoint,
							m_MovementDir;

	int						m_msLevelPassingDuration,
							m_msDoorsOperatingDuration;

	std::mutex				*m_pMutexCout,
							*m_pMutexQueue;
	std::vector<bool>		m_Levels;

public:
	CElevator( std::mutex	&m_cout, std::mutex	&m_queue, int levelsCount, double elevatorSpeed, double levelHeight, int doorsOperatingTime );
	~CElevator();
	void	FindPoint();
	void	MakeMovement();
	bool	GetMessage();
};

CElevator::CElevator( std::mutex	&m_cout, std::mutex	&m_queue, int levelsCount, double elevatorSpeed, double levelHeight, int doorsOperatingDuration )
{
	m_ElevatorCurrentPos		= 0;
	m_MovementPoint				= 0;
	m_MovementDir				= 0;
	m_pMutexCout				= &m_cout;
	m_pMutexQueue				= &m_queue;
	m_Levels.resize(levelsCount);
	m_msLevelPassingDuration	= int( levelHeight / elevatorSpeed * 1000 );
	m_msDoorsOperatingDuration	= doorsOperatingDuration;
}

CElevator::~CElevator()
{
}

void
CElevator::FindPoint()
{
	if (m_ElevatorCurrentPos != m_MovementPoint)
		return;

	int maxLevel = (int)m_Levels.size();

	if (m_MovementDir <= 0)
	{
		for (int i = maxLevel - 1; i >= 0; i--)
		{
			if ( m_Levels[i] ) {
				m_MovementPoint = i;
				break;
			}
		}
	}
	else
	{
		for (int i = 0; i < maxLevel; i++)
		{
			if ( m_Levels[i] ) {
				m_MovementPoint = i;
				break;
			}
		}
	}
}

bool	
CElevator::GetMessage()
{
	while ( qMessages.size( ) )
	{
		m_pMutexQueue->lock();
		ELMESSAGE	msg = qMessages.front();
		qMessages.pop();
		m_pMutexQueue->unlock();
		switch (msg.Code)
		{
			case	EL_TURNOFF:
				return true;
			case	EL_ATLEVEL:
			case	EL_TOLEVEL:
				m_Levels[msg.Value]	= true;
				break;
		}
	}
	return false;
}

void
CElevator::MakeMovement()
{
	if (m_MovementPoint == m_ElevatorCurrentPos)
		return;

	m_MovementDir = (m_MovementPoint - m_ElevatorCurrentPos) / abs(m_MovementPoint - m_ElevatorCurrentPos);

	m_ElevatorCurrentPos += m_MovementDir;
	m_pMutexCout->lock();
	std::this_thread::sleep_for(std::chrono::milliseconds( m_msLevelPassingDuration ) );
	std::cout << "Passing level " << m_ElevatorCurrentPos + 1 << " ..." << std::endl;
	if ( m_Levels[m_ElevatorCurrentPos] )
	{
		std::cout << "Doors are openning..." << std::endl;
		std::this_thread::sleep_for( std::chrono::seconds( m_msDoorsOperatingDuration ) );
		std::cout << "Doors are closing..." << std::endl;
		m_Levels[m_ElevatorCurrentPos] = false;
	}
	m_pMutexCout->unlock();
}

void	
ElevatorThreadWrapper( std::mutex	&mutexCout, std::mutex	&mutexQueue, int levelsCount, double elevatorSpeed, double levelHeight, int doorsOperatingTime)
{
	CElevator	elv( mutexCout, mutexQueue, levelsCount,  elevatorSpeed, levelHeight, doorsOperatingTime);
	while (true)
	{
		if (elv.GetMessage())
			break;
		elv.FindPoint();
		elv.MakeMovement();
	}
}

void
ControThreadlWrapper( std::mutex	&mutexCout, std::mutex	&mutexQueue, int levels_count )
{
	std::string	in;

	while( true )
	{
		std::getline(std::cin, in);
		commandToLevel( in, std::ref(mutexCout), std::ref(mutexQueue), levels_count );
		commandAtLevel( in, std::ref(mutexCout), std::ref(mutexQueue), levels_count);
		if (commandExit( in ) )
			break;
	}
}

int
main(int argc, char* argv[])
{
	if (argc < 5)
	{
		std::cout << "You have to input 4 arguments" << std::endl;
		return 1;
	}

	int		numLevels = 0,
			operTime = 0;

	double	levelHeight = 0,
			elSpeed = 0;

	try 
	{
		getElevatorParams(argv, numLevels, std::ref( elSpeed), std::ref( levelHeight ), std::ref( operTime ) );
	}
	catch (int exc)
	{
		std::cout << "An error occurred in param:" << argv[exc] << std::endl;
		return 1;
	}

	std::cout << "------------------------------------------------" << std::endl;
	std::cout << "Available commands:" << std::endl;
	std::cout << "atXX - to call elevator at level XX:" << std::endl;
	std::cout << "toXX - to push button \"Level XX\" into the elevator" << std::endl;
	std::cout << "exit - to quit the program" << std::endl;
	std::cout << "------------------------------------------------" << std::endl;
	std::cout << "Number of levels:" << numLevels << std::endl;

	std::mutex	mCout,
				mMesQueue;

	std::thread	control_thread( ControThreadlWrapper, std::ref(mCout), std::ref(mMesQueue), numLevels );
	std::thread	elevator_thread( ElevatorThreadWrapper, std::ref(mCout), std::ref(mMesQueue), numLevels, elSpeed, levelHeight, operTime );

	if (control_thread.joinable())
		control_thread.join();
	
	if (elevator_thread.joinable())
		elevator_thread.join();

	return 0;
}

