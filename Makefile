CXX      ?= g++
CXXFLAGS  = -std=c++17 -O2 -Wall -Wextra -pthread -Iinclude
LDFLAGS   = -pthread -lm
DEPFLAGS  = -MMD -MP

# Optional libpcap support: compile with  make LIBPCAP=1
ifdef LIBPCAP
  CXXFLAGS += -DHAVE_LIBPCAP
  LDFLAGS  += -lpcap
endif

TARGET = simulator
OBJDIR = obj

SRCS := main.cpp \
        src/request.cpp \
        src/scheduler.cpp \
        src/server.cpp \
        src/metrics.cpp \
        src/simulation.cpp \
        src/socket_engine.cpp \
        src/congestion.cpp \
        src/packet_scheduler.cpp \
        src/qos.cpp \
        src/trace_replay.cpp \
        src/web_dashboard.cpp \
        src/queueing_model.cpp

OBJS := $(patsubst %.cpp,$(OBJDIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c -o $@ $<

-include $(DEPS)

clean:
	rm -rf $(OBJDIR) $(TARGET)
