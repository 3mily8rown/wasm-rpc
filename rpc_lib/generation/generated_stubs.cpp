bool RpcClient::sendMessage(int32_t id, const char* name) {
    SendMessage msg = SendMessage_init_zero;
    msg.id = id;
    std::strncpy(msg.name, name, sizeof(msg.name) - 1);
    if (!send(next_request_id_++, RpcEnvelope_sendMessage_tag, &msg)) return false;
    SendMessageResponse resp = SendMessageResponse_init_zero;
    if (!receive<SendMessageResponse>(RpcResponse_sendMessage_tag, &resp)) return false;
    return true;
}

bool RpcClient::addRandom(int32_t num) {
    AddRandom msg = AddRandom_init_zero;
    msg.num = num;
    if (!send(next_request_id_++, RpcEnvelope_addRandom_tag, &msg)) return false;
    AddRandomResponse resp = AddRandomResponse_init_zero;
    if (!receive<AddRandomResponse>(RpcResponse_addRandom_tag, &resp)) return false;
    return true;
}

bool RpcClient::processFloats(size_t num_count, const float* num, size_t count) {
    ProcessFloats msg = ProcessFloats_init_zero;
    msg.num_count = num_count;
    if (count > PB_ARRAY_SIZE(&msg, num)) {
        std::fprintf(stderr, "Too many items for num\n");
        return false;
    }
    msg.num_count = count;
    std::memcpy(msg.num, num, count * sizeof(float));
    if (!send(next_request_id_++, RpcEnvelope_processFloats_tag, &msg)) return false;
    ProcessFloatsResponse resp = ProcessFloatsResponse_init_zero;
    if (!receive<ProcessFloatsResponse>(RpcResponse_processFloats_tag, &resp)) return false;
    return true;
}