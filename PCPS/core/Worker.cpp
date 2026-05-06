#include "Worker.h"

double Worker::maxM_ = 30.0 / 32;

void Worker::runRBVHThroughputTest() {
    assert(indexType_ == IndexType::RBvh);
    thread_mem::ThreadMemScope scope;

    double lastMem = 0.0;
    double startTime = thread_real_time(), lastTime = startTime;
    int requestNum = 0;

    double curTime, lastFlush = thread_real_time();
    bool isExit = false;
    Request request;
    while (true) {
        {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [&]() { return !requests_.empty();});

            request = requests_.front();
            requests_.pop();
        }
        if(++requestNum % 100000 == 0){
            curTime = thread_real_time();
            printf("Worker %-2d process 100k / %dk requests in %.3lf / %.3lf secs\n\n",
                   workerID_, requestNum / 1000, curTime - lastTime, curTime - startTime);
            lastTime = curTime;
        }
        if(request.type_ == RequestType::Stop) {
            index_->flush();
            result_ = index_->totalMatch();
            memory_ = scope.used();
            peak_ = scope.peak();
            isExit = true;
        }else if(request.type_ == RequestType::Register){
            index_->flush();
            index_->registerCRQuery(std::get<CRQuery*>(request.payload_));

            if(scope.used() > Worker::maxM_){
                lastMem = scope.used();
                index_->condenseTree(thread_real_time());
                printf("Worker %-2d completes condensing, memory: %.3lf GB -> %.3lf GB\n\n",
                       workerID_, lastMem, scope.used());
            }
            lastFlush = thread_real_time();
        }else if(request.type_ == RequestType::Cancel){
            index_->flush();
            index_->cancelCRQuery(std::get<CRQuery*>(request.payload_));
            lastFlush = thread_real_time();
        }else if(request.type_ == RequestType::Publish){
            curTime = thread_real_time();
            if(curTime - lastFlush > index_->getFlushInterval()){
                index_->flush();
                lastFlush = curTime;
            }
            auto& point = std::get<Point*>(request.payload_);
            index_->publishPoint(point);
            index_->updateSTList(point, curTime);

            if(scope.used() > Worker::maxM_){
                lastMem = scope.used();
                index_->flush();
                index_->condenseTree(curTime);
                lastFlush = thread_real_time();
                printf("Worker %-2d completes condensing, memory: %.3lf GB -> %.3lf GB\n\n",
                       workerID_, lastMem, scope.used());
            }
        }else{
            index_->flush();
            lastFlush = thread_real_time();
        }

        if(isExit) break;
    }

    runTime_ = thread_real_time() - startTime;
}

void Worker::runBaselineTest() {
    thread_mem::ThreadMemScope scope;
    double startTime = thread_real_time(), lastTime = startTime, stageStartTime = startTime;
    int requestNum = 0;

    double lastMem = 0.0;
    bool isExit = false;
    Request request;

    bool inPub = false, inCan = false;
    while (true) {
        {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [&]() { return !requests_.empty();});

            request = requests_.front();
            requests_.pop();
        }
        if(++requestNum % 100000 == 0){
            double curTime = thread_real_time();
            printf("Worker %-2d process 100k / %dk requests in %.3lf / %.3lf secs\n\n",
                   workerID_, requestNum / 1000, curTime - lastTime, curTime - startTime);
            lastTime = curTime;
        }
        if(request.type_ == RequestType::Stop) {
            double curTime = thread_real_time();
            stageTime_[2] = curTime - stageStartTime;
            runTime_ = curTime - startTime;
            result_ = index_->totalMatch();
            memory_ = scope.used();
            peak_ = scope.peak();
            isExit = true;
        }else if(request.type_ == RequestType::Register){
            index_->registerCRQuery(std::get<CRQuery*>(request.payload_));
            if(scope.used() > Worker::maxM_){
                lastMem = scope.used();
                index_->condenseTree(thread_real_time());
                printf("Worker %-2d completes condensing, memory: %.3lf GB -> %.3lf GB\n\n",
                       workerID_, lastMem, scope.used());
            }
        }else if(request.type_ == RequestType::Cancel){
            if(!inCan){
                inCan = true;
                double curTime = thread_real_time();
                stageTime_[1] = curTime - stageStartTime;
                stageStartTime = curTime;
            }
            index_->cancelCRQuery( std::get<CRQuery*>(request.payload_));
        }else if(request.type_ == RequestType::Publish){
            if(!inPub){
                inPub = true;
                double curTime = thread_real_time();
                stageTime_[0] = curTime - stageStartTime;
                stageStartTime = curTime;
            }
            index_->publishPoint(std::get<Point*>(request.payload_));
        }

        if(isExit) break;
    }
}

void Worker::runRBVHRegTest() {
    assert(indexType_ == IndexType::RBvh);
    thread_mem::ThreadMemScope scope;
    double startTime = thread_real_time(), lastTime = startTime;
    int requestNum = 0;
    double lastMem = 0.0, curTime;
    bool isExit = false, inReg = false, tag;
    Request request;
    while (true) {
        {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [&]() { return !requests_.empty();});

            request = requests_.front();
            requests_.pop();
        }
        if(++requestNum % 100000 == 0){
            curTime = thread_real_time();
            printf("Worker %-2d process 100k / %dk requests in %.3lf / %.3lf secs\n\n",
                   workerID_, requestNum / 1000, curTime - lastTime, curTime - startTime);
            lastTime = curTime;
        }
        if(request.type_ == RequestType::Stop) {
            memory_ = scope.used();
            peak_ = scope.peak();
            isExit = true;
        }else if(request.type_ == RequestType::Register){
            index_->registerCRQuery(std::get<CRQuery*>(request.payload_));

            if(scope.used() > Worker::maxM_){
                lastMem = scope.used();
                index_->condenseTree(thread_real_time());
                printf("Worker %-2d completes condensing, memory: %.3lf GB -> %.3lf GB\n\n",
                       workerID_, lastMem, scope.used());
            }
        }else if(request.type_ == RequestType::Publish){
            curTime = thread_real_time();
            if(!inReg){
                inReg = true;
                stageTime_[0] = curTime - startTime;
            }
            auto& point = std::get<Point*>(request.payload_);
            tag = index_->incrementalConstruction(point);
            if(tag) stageTime_[1] += thread_real_time() - curTime;
            index_->updateSTList(point, curTime);

            if(scope.used() > Worker::maxM_){
                lastMem = scope.used();
                index_->condenseTree(curTime);
                printf("Worker %-2d completes condensing, memory: %.3lf GB -> %.3lf GB\n\n",
                       workerID_, lastMem, scope.used());
            }
        }
        if(isExit) break;
    }

    runTime_ = stageTime_[0] + stageTime_[2];
}

void Worker::runRBVHPubTest() {
    assert(indexType_ == IndexType::RBvh);
    thread_mem::ThreadMemScope scope;
    double startTime = thread_real_time(), lastTime = startTime;
    int requestNum = 0;
    double lastMem = 0.0;
    double curTime, lastFlush = thread_real_time();
    bool isExit = false;
    Request request;
    while (true) {
        {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [&]() { return !requests_.empty();});

            request = requests_.front();
            requests_.pop();
        }
        if(++requestNum % 100000 == 0){
            curTime = thread_real_time();
            printf("Worker %-2d process 100k / %dk requests in %.3lf / %.3lf secs\n\n",
                   workerID_, requestNum / 1000, curTime - lastTime, curTime - startTime);
            lastTime = curTime;
        }
        if(request.type_ == RequestType::Stop) {
            index_->flush();
            result_ = index_->totalMatch();
            memory_ = scope.used();
            peak_ = scope.peak();
            isExit = true;
        }else if(request.type_ == RequestType::Register){
            index_->flush();
            index_->registerCRQuery(std::get<CRQuery*>(request.payload_));
            if(scope.used() > Worker::maxM_){
                lastMem = scope.used();
                index_->condenseTree(thread_real_time());
                printf("Worker %-2d completes condensing, memory: %.3lf GB -> %.3lf GB\n\n",
                       workerID_, lastMem, scope.used());
            }
            lastFlush = thread_real_time();
        }else if(request.type_ == RequestType::Publish){
            curTime = thread_real_time();
            if(curTime - lastFlush > index_->getFlushInterval()){
                index_->flush();
                lastFlush = curTime;
            }
            auto& point = std::get<Point*>(request.payload_);
            index_->publishPoint(point);
            index_->updateSTList(point, curTime);

            if(scope.used() > Worker::maxM_){
                lastMem = scope.used();
                index_->flush();
                index_->condenseTree(curTime);
                lastFlush = thread_real_time();
                printf("Worker %-2d completes condensing, memory: %.3lf GB -> %.3lf GB\n\n",
                       workerID_, lastMem, scope.used());
            }
        }else{
            index_->flush();
            lastFlush = thread_real_time();
        }

        if(isExit) break;
    }

    runTime_ = thread_real_time() - startTime;
}

void Worker::runRBVHCanTest() {
    assert(indexType_ == IndexType::RBvh);
    thread_mem::ThreadMemScope scope;
    double startTime = thread_real_time(), lastTime = startTime;
    int requestNum = 0;
    double curTime, lastFlush = thread_real_time();
    bool isExit = false;
    Request request;
    while (true) {
        {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [&]() { return !requests_.empty();});

            request = requests_.front();
            requests_.pop();
        }
        if(++requestNum % 100000 == 0){
            curTime = thread_real_time();
            printf("Worker %-2d process 100k / %dk requests in %.3lf / %.3lf secs\n\n",
                   workerID_, requestNum / 1000, curTime - lastTime, curTime - startTime);
            lastTime = curTime;
        }
        if(request.type_ == RequestType::Stop) {
            index_->flush();
            memory_ = scope.used();
            peak_ = scope.peak();
            result_ = index_->totalMatch();
            isExit = true;
        }else if(request.type_ == RequestType::Cancel){
            index_->flush();
            index_->cancelCRQuery(std::get<CRQuery*>(request.payload_));
        }else if(request.type_ == RequestType::Publish){
            curTime = thread_real_time();
            if(curTime - lastFlush > index_->getFlushInterval()){
                index_->flush();
                lastFlush = curTime;
            }
            auto& point = std::get<Point*>(request.payload_);
            index_->publishSynchronizedDelete(point);
        }else{
            index_->flush();
            lastFlush = thread_real_time();
        }

        if(isExit) break;
    }

    runTime_ = thread_real_time() - startTime;
}

