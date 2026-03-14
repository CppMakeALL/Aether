#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>

const char* TEST_MSG = "Hello from Aether RDMA client!";

int main() {
    struct rdma_cm_id* cm_id = nullptr;
    struct rdma_event_channel* ec = rdma_create_event_channel();
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
    inet_pton(AF_INET, "10.10.0.190", &addr.sin_addr);

    if (rdma_resolve_addr(cm_id, nullptr, (sockaddr*)&addr, 2000)) {
        std::cerr << "Failed to resolve address" << std::endl;
        return 1;
    }

    rdma_cm_event* event;
    if (rdma_get_cm_event(ec, &event)) {
        std::cerr << "Failed to get event" << std::endl;
        return 1;
    }

    if (event->event != RDMA_CM_EVENT_ADDR_RESOLVED) {
        std::cerr << "Address resolution failed, event: " << event->event << std::endl;
        return 1;
    }
    rdma_ack_cm_event(event);
    std::cout << "Address resolved" << std::endl;

    if (rdma_resolve_route(cm_id, 2000)) {
        std::cerr << "Failed to resolve route" << std::endl;
        return 1;
    }

    if (rdma_get_cm_event(ec, &event)) {
        std::cerr << "Failed to get event" << std::endl;
        return 1;
    }

    if (event->event != RDMA_CM_EVENT_ROUTE_RESOLVED) {
        std::cerr << "Route resolution failed, event: " << event->event << std::endl;
        return 1;
    }
    rdma_ack_cm_event(event);
    std::cout << "Route resolved" << std::endl;

    struct ibv_cq* send_cq = ibv_create_cq(cm_id->verbs, 10, nullptr, nullptr, 0);
    struct ibv_cq* recv_cq = ibv_create_cq(cm_id->verbs, 10, nullptr, nullptr, 0);

    struct ibv_qp_init_attr qp_attr{};
    qp_attr.send_cq = send_cq;
    qp_attr.recv_cq = recv_cq;
    qp_attr.qp_type = IBV_QPT_RC;
    qp_attr.cap.max_send_wr = 10;
    qp_attr.cap.max_recv_wr = 10;
    qp_attr.cap.max_send_sge = 1;
    qp_attr.cap.max_recv_sge = 1;

    if (rdma_create_qp(cm_id, nullptr, &qp_attr)) {
        std::cerr << "Failed to create QP" << std::endl;
        return 1;
    }

    struct rdma_conn_param param{};
    param.responder_resources = 1;
    param.initiator_depth = 1;
    param.retry_count = 7;
    param.rnr_retry_count = 7;

    if (rdma_connect(cm_id, &param)) {
        std::cerr << "Failed to connect" << std::endl;
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

    std::cout << "Client connected!" << std::endl;

    char* buf = new char[1024];
    struct ibv_mr* mr = ibv_reg_mr(cm_id->pd, buf, 1024, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);

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
    if (ibv_post_send(cm_id->qp, &send_wr, &bad_send_wr)) {
        std::cerr << "Failed to post send" << std::endl;
        return 1;
    }
    
    struct ibv_wc wc{};
    while (ibv_poll_cq(send_cq, 1, &wc) == 0);
    std::cout << "Client send: " << TEST_MSG << std::endl;

    memset(buf, 0, 1024);
    struct ibv_recv_wr wr{};
    struct ibv_sge sge{};
    sge.addr = (uint64_t)buf;
    sge.length = 1024;
    sge.lkey = mr->lkey;
    wr.sg_list = &sge;
    wr.num_sge = 1;
    
    struct ibv_recv_wr* bad_wr = nullptr;
    if (ibv_post_recv(cm_id->qp, &wr, &bad_wr)) {
        std::cerr << "Failed to post recv" << std::endl;
        return 1;
    }

    while (ibv_poll_cq(recv_cq, 1, &wc) == 0);
    std::cout << "Client recv: " << buf << std::endl;

    sleep(1);
    ibv_dereg_mr(mr);
    delete[] buf;
    rdma_disconnect(cm_id);

    std::cout << "Client done" << std::endl;
    return 0;
}
