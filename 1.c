asmlinkage void __sched schedule(void) 
{
	/*
	 * 定义current前面两个task_struct结构体变量
	 */
	struct task_struct *prev, *next;
	unsigned long *switch_count;
	struct rq *rq;
	int cpu;

need_resched:
	/*
	*关闭内核抢占
	*/
	preempt_disable();
	/*
	*获取当前cpu
	*/
	cpu = smp_processor_id();
	/*
	*得到当前CPU的运行队列
	*/
	rq = cpu_rq(cpu);
	/*
	* 将当前进程保存在prev中
	*/
	rcu_qsctr_inc(cpu);                
	prev = rq->curr;
	/*
	*进程切换计数
	*/
	switch_count = &prev->nivcsw;

	release_kernel_lock(prev);
need_resched_nonpreemptible:

	schedule_debug(prev);

	hrtick_clear(rq);

	/*
	 * Do the rq-clock update outside the rq lock:
	 */
	local_irq_disable();
	/*
	*更新运行队列的clock值
	*/
	update_rq_clock(rq);
	spin_lock(&rq->lock);
	/*
	*清楚抢占标志
	*/
	clear_tsk_need_resched(prev);
	/*
	*如果当前进程不是运行态并且内核态没有被抢占
	*/
	if (prev->state && !(preempt_count() & PREEMPT_ACTIVE)) {
		/*
		*如果当前进程处于等待信号的状态，那么转为就绪态
		*否则，把当前进程移动出就绪队列
		*最后进程切换计数
		*/
		if (unlikely(signal_pending_state(prev->state, prev)))
			prev->state = TASK_RUNNING;
		else
			deactivate_task(rq, prev, 1);
		switch_count = &prev->nvcsw;
	}

#ifdef CONFIG_SMP
	if (prev->sched_class->pre_schedule)
		prev->sched_class->pre_schedule(rq, prev);
#endif
	/*
	*如果当前运行队列中没有正在运行的进程，那么从其他地方寻找进程过来执行
	*/
	if (unlikely(!rq->nr_running))
		idle_balance(cpu, rq);
	/*
	*通知调度器类当前被执行的进程要被另外一个进程所替代，并从就绪队列中虚招合适的进程准备执行
	*/
	prev->sched_class->put_prev_task(rq, prev);
	next = pick_next_task(rq, prev);
	/*
	*如果选择的进程不是原进程，则进行上下文切换
	*否则，只需要解锁即可
	*/
	if (likely(prev != next)) {
		sched_info_switch(prev, next);

		rq->nr_switches++;
		rq->curr = next;
		++*switch_count;
		/*上下文切换*/
		context_switch(rq, prev, next); /* unlocks the rq */
		/*
		 * the context switch might have flipped the stack from under
		 * us, hence refresh the local variables.
		 */
		cpu = smp_processor_id();
		rq = cpu_rq(cpu);
	} else
		spin_unlock_irq(&rq->lock);

	hrtick_set(rq);

	if (unlikely(reacquire_kernel_lock(current) < 0))
		goto need_resched_nonpreemptible;

	preempt_enable_no_resched();
	if (unlikely(test_thread_flag(TIF_NEED_RESCHED)))
        /*如果当前进程的重新调度被设置，则再次进行调度*/
		goto need_resched;
}
