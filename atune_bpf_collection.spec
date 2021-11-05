Name:			A-Tune-BPF-Collection
Version:		0.1
Release:		1
License:		Mulan PSL v2
Summary:		BPF program collection to adjust fine-grained kernel mode to get better performance
URL:			https://gitee.com/openeuler/A-Tune-BPF-Collection
Source0:		https://gitee.com/openeuler/A-Tune-BPF-Collection/repository/archive/v%{version}.tar.gz

BuildRequires: clang, llvm, libbpf-devel
Requires: libbpf, systemd
Provides: readahead_tune

%define  debug_package %{nil}

%description
A-Tune BPF Collection contains a set of BPF program which can interact with kernel in real time.
It has the following capabilities:
readahead_tune: trace file reading characteristics, then ajust file read mode to get maximum I/O efficency

%prep
%autosetup -n %{name}-%{version} -p1

%build
make %{?_smp_mflags}

%install
install -D -p -m 0644 readahead_tune.conf %{buildroot}%{_sysconfdir}/sysconfig/readahead_tune.conf
install -D -p -m 0644 readahead_tune.service %{buildroot}/%{_unitdir}/readahead_tune.service
install -D -p -m 0644 readahead_tune %{buildroot}/%{_sbindir}/readahead_tune

%files
%{_sbindir}/readahead_tune
%{_unitdir}/*.service
%config(noreplace) %{_sysconfdir}/sysconfig/readahead_tune.conf

%changelog
* Tue Nov 9 2021 lvying<lvying6@huawei.com> - 0.1-1
- Type:feature
- ID:NA
- SUG:NA
- DESC: Init A-Tune-BPF-Collection repo and add readahead_tune service 
