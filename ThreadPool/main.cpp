//
//  main.cpp
//  ThreadPool
//
//  Created by Roy Rao on 2020/12/2.
//

#include <string>
#include "threadpool.hpp"

void foo_1() {
    cout << "Executing first foo." << endl;
}

int foo_2() {
    cout << "Executing second foo." << endl;
    return 666;
}

int main(int argc, const char * argv[]) {
    // insert code here...
    try {
        ThreadPool pool { 4 };
        
        pool.resume();
        
        future<void> f1 = pool.commit(foo_1);
        future<int> f2 = pool.commit(foo_2);
        future<string> f3 = pool.commit( []() -> string {
            cout << "Executing third foo." << endl;
            return "third foo";
        });
        
        pool.stop();
        
        f1.get();
        cout << "I can access their return value here: " << f2.get() << "\t" << f3.get() << endl;
        this_thread::sleep_for(chrono::seconds(5));
        
        pool.resume();
        
        // different way of writting
        pool.commit(foo_1).get();
        
        cout << "finished!" << endl;
        
        return 0;
    } catch (exception &e) {
        cout << "something happened: " << e.what() << endl;
    }
}
