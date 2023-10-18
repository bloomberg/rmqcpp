#!/bin/bash

FROM=(100 1000 10000)
M=(9 9 10)
MSG_SIZE=()

for i in {0..2}
do
	val=$((FROM[i]))
	m=$((M[i]))
	for ((j=val;j<=m*val;j+=val))
	do
		MSG_SIZE+=("${j}")
	done
done

for tracing in with without; do
	for msg_size in "${MSG_SIZE[@]}"
	do
		echo "Message size: $msg_size"
		sum=0
		for ((i=1;i<=3;i+=1))
		do
			result=""
			if [ "$1" = "producer" ]; then
				result=$(RABBITMQ_MESSAGE_SIZE=$msg_size TRACING=$tracing ./tests/performance/test_performance_producer.sh | grep "seconds, message rate" | sed 's/.*\, message rate: \([0-9]*\).*/\1/')
			elif [ "$1" = "consumer" ]; then
				result=$(RABBITMQ_MESSAGE_SIZE=$msg_size TRACING=$tracing ./tests/performance/test_performance_consumer.sh | grep "seconds, rate" | sed 's/.*\, rate: \([0-9]*\).*/\1/')
			fi
			echo "Result $i: $result"
			sum=$((sum+result))
		done
		echo "Average $tracing Tracing:  $((sum / 3))"
		echo
	done
done

