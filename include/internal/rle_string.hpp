/*
 * rle_string.hpp
 *
 *  Created on: Dec 1, 2015
 *      Author: nico
 */

#ifndef INCLUDE_INTERNAL_RLE_STRING_HPP_
#define INCLUDE_INTERNAL_RLE_STRING_HPP_

#include "includes.hpp"
#include <typeinfo>

namespace dyn{

template<
	typename char_type,			//character type (any integer type).
								//The class implements
								//some optimizations for bool type.
	class sparse_bitvector_t,	//bitvector taking O(b) words of space,
								//b being the number of bits set
	class string_t				//data structure implementing a string with
								//access/rank/select/insert functionalities.
>
class rle_string{

public:

	/*
	 * Constructor #1
	 *
	 * 2 cases:
	 * - If char_type != bool, the string accepts any alphabet (unknown at priori). Run-heads are are gamma-coded
	 * - Otherwise, alphabet is boolean and fixed: {true, false}
	 *
	 */
	rle_string(){}

	/*
	 * Constructor #2
	 *
	 * We know only alphabet size. Each Run-head char is assigned log2(sigma) bits.
	 * Characters are assigned codes 0,1,2,... in order of appearance
	 *
	 * Warning: cannot call this constructor on bitvector (i.e. char_type==bool)
	 *
	 */
	rle_string(uint64_t sigma){

		assert( typeid(char_type) != typeid(bool) );
		assert(sigma>0);

		//if sigma=2, we don't need run_heads since bits are
		//always alternated in ru_heds
		run_heads_ = string_t(sigma);

	}

	/*
	 * Constructor #3
	 *
	 * We know character probabilities. Input: pairs <character, probability>
	 *
	 * Here Run-heads are Huffman encoded.
	 *
	 * Warning: cannot call this constructor on bitvector (i.e. char_type==bool)
	 *
	 */
	rle_string(vector<pair<char_type,double> >& P){

		assert( typeid(char_type) != typeid(bool) );
		run_heads_ = string_t(P);

	}

	char_type at(ulint i){

		assert(i<n);
		return run_heads_at( runs.rank1(i) );

	}

	char_type operator[](ulint i){

		return at(i);

	}

	/*
	 * position of i-th character c. i starts from 0!
	 */
	ulint select(ulint i, char_type c){

		assert(i < rank(size(),c));

		ulint this_c_run = runs_per_letter[c].rank1(i);

		//position of i-th c inside its c-run
		ulint sel = i - ( this_c_run == 0 ? 0 : runs_per_letter[c].select1(this_c_run-1)+1 );

		//run number among all runs
		ulint this_run = run_heads_select(this_c_run, c);

		sel += (this_run == 0 ? 0 : runs.select1(this_run-1)+1);

		return sel;

	}

	/*
	 * position of i-th 0. i starts from 0 (only for bitvectors!)
	 */
	ulint select0(ulint i){

		assert( typeid(char_type) == typeid(bool) );
		return select(i, false);

	}

	/*
	 * position of i-th 1. i starts from 0 (only for bitvectors!)
	 */
	ulint select1(ulint i){

		assert( typeid(char_type) == typeid(bool) );
		return select(i, true);

	}

	/*
	 * number of c before position i excluded
	 */
	ulint rank(ulint i, char_type c){

		assert(i<=size());

		//this run is the number 'this_run' among all runs
		ulint this_run = runs.rank1(i);

		//this c-run is the number 'this_c_run' among all c-runs
		ulint this_c_run = run_heads_rank(this_run,c);

		//number of cs before position i (excluded) in THIS c-run
		ulint rk = i - (this_run == 0 ? 0 : runs.select1(this_run-1)+1 );

		//add also number of cs before this run (excluded)
		rk += (this_c_run == 0 ? 0 : runs_per_letter[c].select1(this_c_run-1)+1 );

		return rk;

	}

	/*
	 * number of 0s before position i (only for bitvectors!)
	 */
	ulint rank0(ulint i){

		assert( typeid(char_type) == typeid(bool) );
		return rank(i, false);

	}

	/*
	 * number of 1s before position i (only for bitvectors!)
	 */
	ulint rank1(ulint i){

		assert( typeid(char_type) == typeid(bool) );
		return rank(i, true);

	}

	//break range: given a range <l',r'> on the string and a character c, this function
	//breaks <l',r'> in maximal sub-ranges containing character c.
	//for simplicity and efficiency, we assume that characters at range extremities are both 'c'
	//thanks to the encoding (run-length), this function is quite efficient: O(|result|) ranks and selects
	/*vector<range_t> break_range(range_t rn,uchar c){

		auto l = rn.first;
		auto r = rn.second;

		assert(l<=r);
		assert(r<size());

		assert(operator[](l)==c);
		assert(operator[](r)==c);

		//retrieve runs that contain positions l and r
		auto run_l = run_of(l);
		auto run_r = run_of(r);

		//in this case rn contains only character c: do not break
		if(run_l.first==run_r.first) return {rn};

		vector<range_t> result;

		//first range: from l to the end of the run containing position l
		result.push_back({l,run_l.second});

		//rank of c's of interest in run_heads
		ulint rank_l = run_heads.rank(run_l.first,c);
		ulint rank_r = run_heads.rank(run_r.first,c);

		//now retrieve run bounds of all c-runs of interest
		for(ulint j = rank_l+1;j<rank_r;++j){

			result.push_back(run_range(run_heads.select(j,c)));

		}

		//now last (possibly incomplete) run

		auto range = run_range(run_heads.select(rank_r,c));
		result.push_back({range.first,r});

		return result;

	}*/

	ulint size(){return n;}

	/*
	 * return inclusive range of j-th run in the string
	 */
	/*pair<ulint,ulint> run_range(ulint j){

		assert(j<run_heads.size());

		ulint this_block = j/B;
		ulint current_run = this_block*B;
		ulint pos = (this_block==0?0:runs.select(this_block-1)+1);

		while(current_run < j){

 			pos += run_at(current_run);
			current_run++;

		}

		assert(current_run == j);

		return {pos,pos+run_at(j)-1};

	}

	//length of i-th run
	ulint run_at(ulint i){

		assert(i<R);
		uchar c = run_heads_at[i];

		return runs_per_letter[c].gapAt(run_heads_rank(i,c));

	}*/

	ulint number_of_runs(){return R;}

	/* serialize the structure to the ostream
	 * \param out	 the ostream
	 */
	/*ulint serialize(std::ostream& out){

		ulint w_bytes = 0;

		out.write((char*)&n,sizeof(n));
		out.write((char*)&R,sizeof(R));
		out.write((char*)&B,sizeof(B));

		w_bytes += sizeof(n) + sizeof(R) + sizeof(B);

		if(n==0) return w_bytes;

		w_bytes += runs.serialize(out);

		for(ulint i=0;i<256;++i)
			w_bytes += runs_per_letter[i].serialize(out);

		w_bytes += run_heads.serialize(out);

		return w_bytes;

	}*/

	/* load the structure from the istream
	 * \param in the istream
	 */
	/*void load(std::istream& in) {

		in.read((char*)&n,sizeof(n));
		in.read((char*)&R,sizeof(R));
		in.read((char*)&B,sizeof(B));

		if(n==0) return;

		runs.load(in);

		runs_per_letter = vector<sparse_bitvector_t>(256);

		for(ulint i=0;i<256;++i)
			runs_per_letter[i].load(in);

		run_heads.load(in);

	}*/

private:

	char_type run_heads_at(ulint i){

		assert(i<run_heads_size);

		if( typeid(char_type) == typeid(bool) ){

			return (i%2) xor run_heads_first_bit;

		}else{

			return run_heads_[i];

		}

	}

	ulint run_heads_rank(ulint i, char_type c){

		assert(i<=run_heads_size);

		if( typeid(char_type) == typeid(bool) ){

			return (i + (run_heads_first_bit xor not c ) )/2;

		}else{

			return run_heads_.rank(i,c);

		}

	}

	ulint run_heads_select(ulint i, char_type c){

		if( typeid(char_type) == typeid(bool) ){

			assert(i<run_heads_size/2);
			return i*2  + ( c xor run_heads_first_bit );

		}else{

			return run_heads_.select(i,c);

		}

	}

	/*
	 * insert c at position i in run_heads. This operation must
	 * not duplicate any character (i.e. must not create a run
	 * of length >1)
	 */
	void run_heads_insert(ulint i, char_type c){

		assert(i <= run_heads_size);

		//cannnot duplicate a character
		assert(i==0 || run_heads_at(i-1)!=c);
		assert(i==run_heads_size || run_heads_at(i+1)!=c);

		run_heads_size++;

		if( typeid(char_type) == typeid(bool) ){

			//if i==0, then we are flipping the first bit. Otherwise,
			//we only need to increment size.
			run_heads_first_bit = run_heads_first_bit xor (i==0);

		}else{

			run_heads_.insert(i,c);

		}

	}

	/*
	 * split i-th run head: a -> aca
	 */
	void run_heads_split(ulint i, char_type c){

		assert(i < run_heads_size);

		char_type r = run_heads_at(i);

		assert(r!=c);

		run_heads_size += 2;

		if( typeid(char_type) != typeid(bool) ){

			run_heads_.insert(i+1,r);
			run_heads_.insert(i+1,c);

		}
		/*
		 * else do nothing because first character
		 * does not change
		 */

	}

	//<j=run of position i, last position of j-th run>
	/*pair<ulint,ulint> run_of(ulint i){

		ulint last_block = runs.rank(i);
		ulint current_run = last_block*B;

		//current position in the string: the first of a block
		ulint pos = 0;
		if(last_block>0)
			pos = runs.select(last_block-1)+1;

		assert(pos <= i);

		while(pos < i){

 			pos += run_at(current_run);
			current_run++;

		}

		assert(pos >= i);

		if(pos>i){

			current_run--;

		}else{//pos==i

			pos += run_at(current_run);

		}

		assert(pos>0);
		assert(current_run<R);

		return {current_run,pos-1};

	}*/

	//main bitvector storing all run lengths. R bits set
	//a run of length n+1 is stored as 0^n1
	sparse_bitvector_t runs;

	//for each letter, its runs stored contiguously
	map<char_type,sparse_bitvector_t> runs_per_letter;

	//store run heads in a compressed string supporting access/rank/select/insert
	string_t run_heads_;
	ulint run_heads_size=0;
	bool run_heads_first_bit=false;

	//text length and number of runs
	ulint n=0;
	ulint R=0;

};

}

#endif /* INCLUDE_INTERNAL_RLE_STRING_HPP_ */