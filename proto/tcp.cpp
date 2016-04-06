#include "tcp.hpp"

// class tcp_flags
tcp_flags::tcp_flags()
	: enable(false), bits_(std::string("000000")), mask_(std::string("000000")) {}
tcp_flags::tcp_flags(std::pair<std::bitset<6>, std::bitset<6>> flags)
	: enable(true), bits_(flags.first), mask_(flags.second) {}
bool tcp_flags::in_this(const std::bitset<6>& flags) const
{
	if((flags&mask_) == bits_)
		return true;
	return false;
}
bool tcp_flags::operator==(tcp_flags const & other) const
{
	return (bits_ == other.bits_ && mask_ == other.mask_ && enable == other.enable);
}

// class tcp_rule
tcp_rule::tcp_rule()
	: ip_header_r(6), ip_rule() {}
tcp_rule::tcp_rule(std::vector<std::string> tkn_rule)
	: ip_header_r(6), ip_rule(tkn_rule) {}
void tcp_rule::parse(boost::program_options::options_description& opt)
{
	parser::command_parser cp(opt);
	boost::program_options::variables_map vm = cp.parse(tokenize_rule);
	// // check conflicting
	// parser::conflicting_options(vm, "seq-eq", "seq-gt", "seq-lt");
	// parser::conflicting_options(vm, "win-eq", "win-gt", "win-lt");
	// parser::conflicting_options(vm, "ack-eq", "ack-gt", "ack-lt");
	// store text rule
	text_rule = cp.join(tokenize_rule);
	// parse L3 header
	ip_header_parse(vm);
	// parse rule options
	ip_rule_parse(vm);
	// parse L4 header
	if (vm.count("sport")) {
		src_port = parser::range_from_port_string(vm["sport"].as<std::string>());
	}
	if (vm.count("dport")) {
		dst_port = parser::range_from_port_string(vm["dport"].as<std::string>());
	}
	if (vm.count("seq")) {
		seq = parser::numcomp_from_string<uint32_t>(vm["seq"].as<std::string>());
	}
	if (vm.count("ack")) {
		ack_seq = parser::numcomp_from_string<uint32_t>(vm["ack"].as<std::string>());
	}
	if (vm.count("win")) {
		win = parser::numcomp_from_string<uint16_t>(vm["win"].as<std::string>());
	}
	if (vm.count("hlen")) {
		len = parser::numcomp_from_string<uint16_t>(vm["hlen"].as<std::string>());
	}
	if (vm.count("tcp-flag")) {
		std::string flag_opt = vm["tcp-flag"].as<std::string>();
		flags = parser::bitset_from_string<std::bitset<6>>(flag_opt, tcprule::accept_tcp_flags);
	}
}
bool tcp_rule::check_packet(struct tcphdr *tcp_hdr, uint32_t s_addr, uint32_t d_addr) const
{
	// L3 header check
	if(!ip_src.in_this(s_addr)) // check source ip address
		return false;
	if(!ip_dst.in_this(d_addr)) // check destination ip address
		return false;
	// L4 header check
	uint16_t h_sport = ntohs(tcp_hdr->source);
	if(!src_port.in_this(h_sport))
		return false;
	uint16_t h_dport = ntohs(tcp_hdr->dest);
	if(!dst_port.in_this(h_dport))
		return false;
	uint32_t h_seq = ntohl(tcp_hdr->seq);
	if(!seq.in_this(h_seq))
		return false;
	uint32_t h_ack = ntohl(tcp_hdr->ack_seq);
	if(!ack_seq.in_this(h_ack))
		return false;
	uint16_t h_win = ntohs(tcp_hdr->window);
	if(!win.in_this(h_win))
		return false;
	uint16_t h_len = tcp_hdr->doff * 4;
	if(!len.in_this(h_len))
		return false;
	if(flags.enable)
	{
		std::bitset<6> h_flags;
		h_flags[0] = tcp_hdr->urg;
		h_flags[1] = tcp_hdr->ack;
		h_flags[2] = tcp_hdr->psh;
		h_flags[3] = tcp_hdr->rst;
		h_flags[4] = tcp_hdr->syn;
		h_flags[5] = tcp_hdr->fin;
		if(!flags.in_this(h_flags))
			return false;
	}

	// std::cout << "\n\n== IP HEADER ==";
	// std::cout << "\nSource IP: " << boost::asio::ip::address_v4(s_addr).to_string();
	// std::cout << "\nDestination IP: " << boost::asio::ip::address_v4(d_addr).to_string();
	// // TCP Header
	// std::cout << "\n== TCP HEADER ==";
	// std::cout << "\nSource Port: " << std::dec << h_sport;
	// std::cout << "\nDestination Port: " << std::dec << h_dport;
	// std::cout << "\nSEQ number: " << std::dec << ntohl(tcp_hdr->seq);
	// std::cout << "\nACK number: " << std::dec << ntohl(tcp_hdr->ack_seq);
	// std::cout << "\nHeader lenght: " << tcp_hdr->doff * 4;
	// std::cout << "\nURG flag: " << tcp_hdr->urg;
	// std::cout << "\nACK flag: " << tcp_hdr->ack;
	// std::cout << "\nPSH flag: " << tcp_hdr->psh;
	// std::cout << "\nRST flag: " << tcp_hdr->rst;
	// std::cout << "\nSYN flag: " << tcp_hdr->syn;
	// std::cout << "\nFIN flag: " << tcp_hdr->fin;
	// std::cout << "\nWindow size: " << std::dec << ntohs(tcp_hdr->window);
	// std::cout << "\nChecksum: " << std::hex << ntohs(tcp_hdr->check);

	return true;
}
bool tcp_rule::operator==(tcp_rule const & other) const
{
	return (src_port == other.src_port
		&& dst_port == other.dst_port
		&& ip_src == other.ip_src
		&& ip_dst == other.ip_dst
		&& next_rule == other.next_rule
		&& pps_trigger == other.pps_trigger
		&& bps_trigger == other.bps_trigger
		&& pps_trigger_period == other.pps_trigger_period
		&& bps_trigger_period == other.bps_trigger_period
		&& seq == other.seq
		&& ack_seq == other.ack_seq
		&& win == other.win
		&& len == other.len
		&& flags == other.flags);
}
tcp_rule& tcp_rule::operator+=( tcp_rule& other)
{
	if (this != &other)
	{
		count_packets += other.count_packets;
		count_bytes += other.count_bytes;
		// сбрасываем счетчик у исходного правила
		other.count_packets = 0; 
		other.count_bytes = 0;
	}
	return *this;
}
std::string tcp_rule::make_info()
{
	std::string info = "tcp|"
				+ ip_rule_info() + "|"
				+ (ip_src.stat() ? ip_src.to_cidr() : "") + "|"
				+ (ip_dst.stat() ? ip_dst.to_cidr() : "") + "|"
				+ (src_port.stat() ? src_port.to_range() : "") + "|"
				+ (dst_port.stat() ? dst_port.to_range() : "") + "|";
	return info;
}