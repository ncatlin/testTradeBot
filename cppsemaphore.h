#pragma once
//https://gist.github.com/sguzman/9594227
#include <mutex>
#include <condition_variable>
using namespace std;

class semaphore
{
public:

	semaphore(int count_ = 0) : count{ count_ }
	{}

	void notify()
	{
		unique_lock<mutex> lck(mtx);
		++count;
		cv.notify_one();
	}

	void wait()
	{
		unique_lock<mutex> lck(mtx);
		while (count == 0)
		{
			cv.wait(lck);
		}

		--count;
	}

	bool empty()
	{
		return count == 0;
	}


private:

	mutex mtx;
	condition_variable cv;
	int count;
};