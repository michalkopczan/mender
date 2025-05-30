// Copyright 2023 Northern.tech AS
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.

#ifndef MENDER_UPDATE_DAEMON_STATES_HPP
#define MENDER_UPDATE_DAEMON_STATES_HPP

#include <common/io.hpp>
#include <common/optional.hpp>
#include <common/state_machine.hpp>

#include <artifact/artifact.hpp>

#include <mender-update/daemon/context.hpp>
#include <mender-update/daemon/state_events.hpp>

#include <artifact/v3/scripts/executor.hpp>

namespace mender {
namespace update {
namespace daemon {

using namespace std;

namespace io = mender::common::io;
namespace sm = mender::common::state_machine;

namespace artifact = mender::artifact;

namespace script_executor = ::mender::artifact::scripts::executor;

using StateType = sm::State<Context, StateEvent>;

class EmptyState : virtual public StateType {
public:
	void OnEnter(Context &ctx, sm::EventPoster<StateEvent> &poster) override;
};

class InitState : virtual public StateType {
public:
	void OnEnter(Context &ctx, sm::EventPoster<StateEvent> &poster) override;
};

class IdleState : virtual public StateType {
public:
	void OnEnter(Context &ctx, sm::EventPoster<StateEvent> &poster) override;
};

class ScheduleNextPollState : virtual public StateType {
public:
	ScheduleNextPollState(
		events::Timer &timer, const string &poll_action, const StateEvent event, int interval);

	void OnEnter(Context &ctx, sm::EventPoster<StateEvent> &poster) override;

private:
	events::Timer &timer_;
	const string poll_action_;
	const StateEvent event_;
	int interval_;
};

class PollForDeploymentState : virtual public StateType {
public:
	PollForDeploymentState(int retry_interval_seconds, int retry_count);

	void OnEnter(Context &ctx, sm::EventPoster<StateEvent> &poster) override;

private:
	void HandlePollingError(Context &ctx, sm::EventPoster<StateEvent> &poster);
	http::ExponentialBackoff backoff_;
};

class SubmitInventoryState : virtual public StateType {
public:
	SubmitInventoryState(int retry_interval_seconds, int retry_count);
	void OnEnter(Context &ctx, sm::EventPoster<StateEvent> &poster) override;

private:
	void HandlePollingError(Context &ctx, sm::EventPoster<StateEvent> &poster);
	http::ExponentialBackoff backoff_;
};

class SaveState : virtual public StateType {
public:
	// Sub states should implement OnEnterSaveState instead, since we do state saving in
	// here. Note that not all states that participate in an update are SaveStates that get
	// their database key saved. Some states are not because it's good enough to rely on the
	// previously saved state.
	void OnEnter(Context &ctx, sm::EventPoster<StateEvent> &poster) override final;

	virtual void OnEnterSaveState(Context &ctx, sm::EventPoster<StateEvent> &poster) = 0;
	virtual const string &DatabaseStateString() const = 0;
	virtual bool IsFailureState() const = 0;
};

class UpdateDownloadState : virtual public StateType {
public:
	void OnEnter(Context &ctx, sm::EventPoster<StateEvent> &poster) override;

private:
	// `static` since they only need the arguments, but are still strongly tied to
	// OnEnterSaveState.
	static void ParseArtifact(Context &ctx, sm::EventPoster<StateEvent> &poster);
	static void DoDownload(Context &ctx, sm::EventPoster<StateEvent> &poster);
};

class UpdateDownloadCancelState : virtual public StateType {
public:
	void OnEnter(Context &ctx, sm::EventPoster<StateEvent> &poster) override;
};

class SendStatusUpdateState : virtual public StateType {
public:
	// Ignore-failure version.
	SendStatusUpdateState(optional<deployments::DeploymentStatus> status);
	// Retry-then-fail version.
	SendStatusUpdateState(
		optional<deployments::DeploymentStatus> status,
		events::EventLoop &event_loop,
		int retry_interval_seconds,
		int retry_count);
	void OnEnter(Context &ctx, sm::EventPoster<StateEvent> &poster) override;

	// For tests.
	void SetSmallestWaitInterval(chrono::milliseconds interval);

private:
	void DoStatusUpdate(Context &ctx, sm::EventPoster<StateEvent> &poster);

	enum class FailureMode {
		Ignore,
		RetryThenFail,
	};

	optional<deployments::DeploymentStatus> status_;
	FailureMode mode_;
	struct Retry {
		http::ExponentialBackoff backoff;
		events::Timer wait_timer;
	};
	optional<Retry> retry_;
};

class UpdateInstallState : virtual public StateType {
public:
	void OnEnter(Context &ctx, sm::EventPoster<StateEvent> &poster) override;
};

class UpdateCheckRebootState : virtual public StateType {
public:
	void OnEnter(Context &ctx, sm::EventPoster<StateEvent> &poster) override;
};

class UpdateRebootState : virtual public StateType {
public:
	void OnEnter(Context &ctx, sm::EventPoster<StateEvent> &poster) override;
};

class UpdateVerifyRebootState : virtual public SaveState {
public:
	void OnEnterSaveState(Context &ctx, sm::EventPoster<StateEvent> &poster) override;
	const string &DatabaseStateString() const override {
		return Context::kUpdateStateArtifactVerifyReboot;
	}
	bool IsFailureState() const override {
		return false;
	}
};

class UpdateBeforeCommitState : virtual public StateType {
public:
	void OnEnter(Context &ctx, sm::EventPoster<StateEvent> &poster) override;
};

class UpdateCommitState : virtual public StateType {
public:
	void OnEnter(Context &ctx, sm::EventPoster<StateEvent> &poster) override;
};

class UpdateAfterCommitState : virtual public SaveState {
public:
	void OnEnterSaveState(Context &ctx, sm::EventPoster<StateEvent> &poster) override;
	const string &DatabaseStateString() const override {
		return Context::kUpdateStateAfterArtifactCommit;
	}
	bool IsFailureState() const override {
		return false;
	}
};

class UpdateCheckRollbackState : virtual public StateType {
public:
	void OnEnter(Context &ctx, sm::EventPoster<StateEvent> &poster) override;
};

class UpdateRollbackState : virtual public StateType {
public:
	void OnEnter(Context &ctx, sm::EventPoster<StateEvent> &poster) override;
};

class UpdateRollbackRebootState : virtual public StateType {
public:
	void OnEnter(Context &ctx, sm::EventPoster<StateEvent> &poster) override;
};

class UpdateVerifyRollbackRebootState : virtual public SaveState {
public:
	void OnEnterSaveState(Context &ctx, sm::EventPoster<StateEvent> &poster) override;
	const string &DatabaseStateString() const override {
		return Context::kUpdateStateArtifactVerifyRollbackReboot;
	}
	bool IsFailureState() const override {
		return true;
	}
};

class UpdateRollbackSuccessfulState : virtual public StateType {
public:
	void OnEnter(Context &ctx, sm::EventPoster<StateEvent> &poster) override;
};

class UpdateFailureState : virtual public SaveState {
public:
	void OnEnterSaveState(Context &ctx, sm::EventPoster<StateEvent> &poster) override;
	const string &DatabaseStateString() const override {
		return Context::kUpdateStateArtifactFailure;
	}
	bool IsFailureState() const override {
		return true;
	}
};

class UpdateSaveProvidesState : virtual public StateType {
public:
	void OnEnter(Context &ctx, sm::EventPoster<StateEvent> &poster) override;
};

class UpdateCleanupState : virtual public SaveState {
public:
	void OnEnterSaveState(Context &ctx, sm::EventPoster<StateEvent> &poster) override;
	const string &DatabaseStateString() const override {
		return Context::kUpdateStateCleanup;
	}
	bool IsFailureState() const override {
		// This is actually both a failure and non-failure state, but it is executed in
		// every failure scenario, which is what is important here.
		return true;
	}
};

class ClearArtifactDataState : virtual public StateType {
public:
	void OnEnter(Context &ctx, sm::EventPoster<StateEvent> &poster) override;
};

class StateLoopState : virtual public StateType {
public:
	void OnEnter(Context &ctx, sm::EventPoster<StateEvent> &poster) override;
};

class EndOfDeploymentState : virtual public StateType {
public:
	void OnEnter(Context &ctx, sm::EventPoster<StateEvent> &poster) override;
};

class ExitState : virtual public StateType {
public:
	ExitState(events::EventLoop &event_loop);
	void OnEnter(Context &ctx, sm::EventPoster<StateEvent> &poster) override;

	error::Error exit_error;

#ifndef NDEBUG
	void ExitAfter(int number) {
		iterations_left_ = number;
	}
#endif

private:
	events::EventLoop &event_loop_;

#ifndef NDEBUG
	int iterations_left_ {1};
#endif
};


//
// State Script states
//

class StateScriptState : virtual public StateType {
public:
	StateScriptState(
		events::EventLoop &event_loop,
		script_executor::State state,
		script_executor::Action action,
		chrono::seconds script_timeout,
		chrono::seconds retry_interval,
		chrono::seconds retry_timeout,
		const string &artifact_script_path,
		const string &rootfs_script_path,
		script_executor::OnError on_error_) :
		script_ {
			event_loop,
			script_timeout,
			retry_interval,
			retry_timeout,
			artifact_script_path,
			rootfs_script_path,
		},
		state_ {state},
		action_ {action},
		on_error_ {on_error_} {};

	void OnEnter(Context &ctx, sm::EventPoster<StateEvent> &poster) override;

private:
	script_executor::ScriptRunner script_;
	script_executor::State state_;
	script_executor::Action action_;
	script_executor::OnError on_error_;
};

class SaveStateScriptState : virtual public SaveState {
public:
	SaveStateScriptState(
		events::EventLoop &event_loop,
		script_executor::State state,
		script_executor::Action action,
		chrono::seconds script_timeout,
		chrono::seconds retry_interval,
		chrono::seconds retry_timeout,
		const string &artifact_script_path,
		const string &rootfs_script_path,
		const string &database_key,
		const script_executor::OnError on_script_error,
		const bool is_failure_state = false) :
		state_script_state_ {
			event_loop,
			state,
			action,
			script_timeout,
			retry_interval,
			retry_timeout,
			artifact_script_path,
			rootfs_script_path,
			on_script_error,
		},
		database_key_ {database_key},
		is_failure_state_ {is_failure_state} {};

	void OnEnterSaveState(Context &ctx, sm::EventPoster<StateEvent> &poster) override;

	const string &DatabaseStateString() const override {
		return database_key_;
	}

	bool IsFailureState() const override {
		return is_failure_state_;
	}

private:
	StateScriptState state_script_state_;
	const string database_key_;
	const bool is_failure_state_;
};


//
// End State Script states
//


namespace deployment_tracking {

class NoFailuresState : virtual public StateType {
public:
	void OnEnter(Context &ctx, sm::EventPoster<StateEvent> &poster) override;
};

class FailureState : virtual public StateType {
public:
	void OnEnter(Context &ctx, sm::EventPoster<StateEvent> &poster) override;
};

class RollbackAttemptedState : virtual public StateType {
public:
	void OnEnter(Context &ctx, sm::EventPoster<StateEvent> &poster) override;
};

class RollbackFailedState : virtual public StateType {
public:
	void OnEnter(Context &ctx, sm::EventPoster<StateEvent> &poster) override;
};

} // namespace deployment_tracking

} // namespace daemon
} // namespace update
} // namespace mender

#endif // MENDER_UPDATE_DAEMON_STATES_HPP
