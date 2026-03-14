#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>

const char* TEST_MSG = "Hello from Aether RDMA server!";

int main() {
    struct rdma_cm_id* cm_id = nullptr;
    struct rdma_event_channel* ec = nullptr;
    struct ibv_mr* mr = nullptr;
    char* buf = nullptr;

    ec = rdma_create_event_channel();
    if (!ec) {
        std::cerr << "Failed to create event channel" << std::endl;
        return 1;
    }

    if (rdma_create_id(ec, &cm_id, nullptr, RDMA_PS_TCP)) {
        std::cerr << "Failed to create ID" << std::endl;
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(20000);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (rdma_bind_addr(cm_id, (sockaddr*)&addr)) {
        std::cerr << "Failed to bind address" << std::endl;
        return 1;
    }

    if (rdma_listen(cm_id, 10)) {
        std::cerr << "Failed to listen" << std::endl;
        return 1;
    }
    std::cout << "Aether RDMA server listening on port 20000..." << std::endl;

    rdma_cm_event* event;
    if (rdma_get_cm_event(ec, &event)) {
        std::cerr << "Failed to get event" << std::endl;
        return 1;
    }

    if (event->event != RDMA_CM_EVENT_CONNECT_REQUEST) {
        std::cerr << "Unexpected event: " << event->event << std::endl;
        return 1;
    }

    auto conn_id = event->id;
    rdma_ack_cm_event(event);

    struct ibv_cq* send_cq = ibv_create_cq(conn_id->verbs, 10, nullptr, nullptr, 0);
    struct ibv_cq* recv_cq = ibv_create_cq(conn_id->verbs, 10, nullptr, nullptr, 0);

    struct ibv_qp_init_attr qp_attr{};
    qp_attr.send_cq = send_cq;
    qp_attr.recv_cq = recv_cq;
    qp_attr.qp_type = IBV_QPT_RC;
    qp_attr.cap.max_send_wr = 10;
    qp_attr.cap.max_recv_wr = 10;
    qp_attr.cap.max_send_sge = 1;
    qp_attr.cap.max_recv_sge = 1;

    if (rdma_create_qp(conn_id, nullptr, &qp_attr)) {
        std::cerr << "Failed to create QP" << std::endl;
        return 1;
    }

    struct rdma_conn_param param{};
    param.responder_resources = 1;
    param.initiator_depth = 1;
    param.retry_count = 7;
    param.rnr_retry_count = 7;

    if (rdma_accept(conn_id, &param)) {
        std::cerr << "Failed to accept connection" << std::endl;
        return 1;
    }

    if (rdma_get_cm_event(ec, &event)) {
        std::cerr << "Failed to get event" << std::endl;
        return 1;
    }

    if (event->event != RDMA_CM_EVENT_ESTABLISHED) {
        std::cerr << "Connection not established, event: " << event->event << std::endl;
        return 1;
    }
    rdma_ack_cm_event(event);
    std::cout << "Server connection established!" << std::endl;

    buf = new char[1024];
    mr = ibv_reg_mr(conn_id->pd, buf, 1024, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    
    struct ibv_recv_wr wr{};
    struct ibv_sge sge{};
    sge.addr = (uint64_t)buf;
    sge.length = 1024;
    sge.lkey = mr->lkey;
    wr.sg_list = &sge;
    wr.num_sge = 1;
    
    struct ibv_recv_wr* bad_wr = nullptr;
    if (ibv_post_recv(conn_id->qp, &wr, &bad_wr)) {
        std::cerr << "Failed to post recv" << std::endl;
        return 1;
    }

    struct ibv_wc wc{};
    while (ibv_poll_cq(recv_cq, 1, &wc) == 0);
    std::cout << "Server recv: " << buf << std::endl;

    memset(buf, 0, 1024);
    strcpy(buf, TEST_MSG);
    
    struct ibv_send_wr send_wr{};
    struct ibv_sge send_sge{};
    send_sge.addr = (uint64_t)buf;
    send_sge.length = strlen(TEST_MSG);
    send_sge.lkey = mr->lkey;
    send_wr.sg_list = &send_sge;
    send_wr.num_sge = 1;
    send_wr.opcode = IBV_WR_SEND;
    send_wr.send_flags = IBV_SEND_SIGNALED;
    
    struct ibv_send_wr* bad_send_wr = nullptr;
    if (ibv_post_send(conn_id->qp, &send_wr, &bad_send_wr)) {
        std::cerr << "Failed to post send" << std::endl;
        return 1;
    }
    
    while (ibv_poll_cq(send_cq, 1, &wc) == 0);
    std::cout << "Server send: " << TEST_MSG << std::endl;

    sleep(1);
    ibv_dereg_mr(mr);
    delete[] buf;
    rdma_disconnect(conn_id);

    std::cout << "Server done" << std::endl;
    return 0;
}
