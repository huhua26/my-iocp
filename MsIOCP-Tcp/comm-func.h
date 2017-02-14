#ifndef COMM_FUNC_H_
#define COMM_FUNC_H_

//template interface
template<typename T>
std::string T2string( T t )
{
	std::stringstream ss;
	ss << t;
	std::string str = ss.str();
	return str;
}

template<typename LIST1, typename LIST2>
void MergeList(LIST1 &list1, const LIST2 &list2)
{
	typename LIST2::const_iterator it = list2.begin();
	for (; it != list2.end(); ++it)
	{
		list1.push_back(*it);
	}
}


#endif