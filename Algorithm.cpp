//Thanks
//https://zenn.dev/tori_rakko/articles/a77608eda046a0
//https://github.com/thun-c/GameSearch/blob/master/source/AlternateGame.cpp#L44

//memo****************************************************//
//oxゲームを題材に、クラス、ゲームAIおよびWEBでの処理の勉強のため
//oxゲームは、通常モードに加えて、ランダムに１つoかxを消すモードも作る
//CPUの強さは、1,99 
//1は弱い（ランダム)、99はminimaxで完全読み
//MCTS も(deleteモードではMCTSとする？)

//memo****************************************************//


//Ver    : 0.1.0
//Author : cg_er



#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <iomanip>
#include <math.h>
#include <regex>
#include <list>
#include <iterator>
#include <tuple>
#include <set>
#include <array>
#include <unordered_set>
#include <cctype>
#include <iomanip>
#include <numeric>
#include <cassert>
#include <random>
#include <chrono>
#include <sstream>
#include <queue>
#include <deque>
using namespace std;

void print(){std::cout << '\n'; };
template <class T> void print(const T &value){std::cout << value;print();};


template <typename T>
void print_vec(const vector<T>& v)
{
    for (const T& elem : v)
    {
        cout << elem << endl;
    }
    
} 


//定数
constexpr int BOARD_SIZE = 3; //ボードのサイズは3 * 3固定
constexpr int NORMAL_MODE = 0;
constexpr int DELETE_MODE = 1;
constexpr int CPU_MAX_THINK_TIME = 1000; //ms
constexpr int FIRST_PLAYER = 0;
constexpr int SECOND_PLAYER = 1;
constexpr int INF = 1000000000LL;
constexpr int CPU_WEAK = 1;
constexpr int CPU_STRONG = 99;
constexpr int MAX_PUT_OX = 6; //delete_mode用 最大put数

//乱数
std::random_device rnd;
std::mt19937 mt(rnd());

//********クラス定義*************************************************

//時間管理クラス
//https://github.com/thun-c/GameSearch/blob/master/source/AlternateGame.cpp#L44
class TimeKeeper{
private:
    std::chrono::high_resolution_clock::time_point start_time_;
    int64_t time_threshold_;

public:
    TimeKeeper(const int64_t& time_threshold)
    : start_time_(std::chrono::high_resolution_clock::now()),
    time_threshold_(time_threshold){}

    bool isTimeOver() const {
        auto diff = std::chrono::high_resolution_clock::now() - this->start_time_;
        return std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() >= time_threshold_;
    }
};



//ゲーム設定データクラス
class GameSetup{
public:
    int boardSize;
    int game_mode; //0 : normal , 1 : randam_delete
    bool player1_isCPU;
    int player1_CPUstrength;
    bool player2_isCPU;
    int player2_CPUstrength;
};

//プレイヤーデータクラス
class Player{
public:
    string name;
    bool isCPU;
    int cpuStrength;
};

//ゲーム状態
class GameState{
private:
	//盤面情報
    std::vector<int>pieces_;      //first_player 
	std::vector<int>enemy_pieces_;//second_player 

    int boardSize;
    int game_mode;

    //操作記録
    deque<int> que_transaction;

    //プレイヤー情報
    vector<Player> players;
    int currentPlayerID;   //0: 先手番 1: 後手番

	//駒の数を計算する
	int pieceCount(const std::vector<int>& pieces) const {
		int count = 0;
		for (const auto i : pieces) {
			if (i == 1)++count;
		}
		return count;
	}

	//敵が3目並んだか判定する
	bool enemyIsComplete(int x, int y, const int dx, const int dy)const {
		for (int k = 0; k < 3; k++) {
			if (y < 0 || 2 < y || x < 0 || 2 < x ||
				this->enemy_pieces_[x + y * 3] == 0
				)return false;
			x += dx; y += dy;
		}
		return true;
	}


public:
	GameState(
		const std::vector<int>& pieces = std::vector<int>(9),
		const std::vector<int>& enemy_pieces = std::vector<int>(9)
	) :
		pieces_(pieces),
		enemy_pieces_(enemy_pieces)
	{	}

    //初期化(コンストラクタ以外)
    int init(const GameSetup &setup){
        this->game_mode = setup.game_mode;
        this->boardSize = setup.boardSize;
        this->currentPlayerID = 0;
        //プレイヤー情報
        players.resize(2);
        players[0].isCPU = setup.player1_isCPU;
        players[0].cpuStrength = setup.player1_CPUstrength;
        players[1].isCPU = setup.player2_isCPU;
        players[1].cpuStrength = setup.player2_CPUstrength;
        if (players[0].isCPU && players[1].isCPU) {
            players[0].name = "コンピュータ１";
            players[1].name = "コンピュータ２";
        }
        else if (players[0].isCPU && !players[1].isCPU) {
            players[0].name = "コンピュータ";
            players[1].name = "あなた";
        }
        else if (!players[0].isCPU && players[1].isCPU) {
            players[0].name = "あなた";
            players[1].name = "コンピュータ";
        }
        else {
            players[0].name = "プレイヤー１";
            players[1].name = "プレイヤー２";
        }

        return 0;

    }

    //現在のプレイヤーの名前を取得
    string getCurrentPlayerName() const {
        return players[currentPlayerID].name;
    }

    //現在のプレイヤーがCPUか
    bool isCurrentPlayerCPU() const {
        return players[currentPlayerID].isCPU;
    }

    //現在の手番がプレイヤーか
    bool isPlayerTurn() const {
        if (this->players[this->currentPlayerID].isCPU) return false;
        return true;
    }

    //現在の手番が先手か
    bool isFirstPlayerTurn() const {
        if (this->currentPlayerID ==  FIRST_PLAYER) return true;
        return false;
    }

    //ゲームモードを取得
    int getGameMode() const {
        return this->game_mode;
    }

    //現在のゲームモードがNORMALMODEか
    bool isNormalMode() const {
        return (getGameMode() == NORMAL_MODE);
    }

    //現在のゲームモードがDELETEMODEか
    bool isDeleteMode() const {
        return (getGameMode() == DELETE_MODE);
    }


    //プレイヤーの名前を取得
    string getPlayerName(int id) const {
        return this->players[id].name;
    }

    //勝者の名前を取得
    string getWinnerName() const {
        return (this->isPlayer1Win() ? players[FIRST_PLAYER].name : players[SECOND_PLAYER].name);
    }

    //現在の手番のCPUの強さを取得
    int getCPULevel() const {
        return this->players[this->currentPlayerID].cpuStrength;
    }

    //後で実装する
    //勝負中の評価(delete_mode用)
    int evaluate(){
        //敵のマークが２つある場合かつ１つは空(両方とも次は消えない) -1
        //敵のマークが２つある場合(両方とも次は消えない) かつ　次消える自分のマスで３つ揃っている　-10

        return 0;
    }

	// 現在のプレイヤー視点の盤面評価をする
    // 通常モードで、IDの深さが小さいときは、return 0を返す可能性があるが、考えない

	int getScore() {
		if (this->isLose())return -1;
		if (this->isDraw())return 0;

        //delete_modeの場合の評価
        if (this->isDeleteMode()) return this->evaluate();

		return 0; 
	}

	// 現在のプレイヤーが負けたか判定する
	bool isLose()const {
		if (enemyIsComplete(0, 0, 1, 1) || enemyIsComplete(0, 2, 1, -1))return true;
		for (int i = 0; i < 3; i++) {
			if (enemyIsComplete(0, i, 1, 0) || enemyIsComplete(i, 0, 0, 1))
				return true;
		}
		return false;
	}

	//  引き分けになったか判定する
	bool isDraw()const {
        if (isLose()) return false; //9個のときに、勝負ついていれば引き分け判定させないように
		return this->pieceCount(this->pieces_) + this->pieceCount(this->enemy_pieces_) == 9;
	}

	// ゲームが終了したか判定する
	bool isDone()const {
		return this->isLose() || this->isDraw();
	}

    //プレイヤー1が勝利したか
    //ゲームの終了後に、advanceメソッドで次のプレイヤー番号になるので反転させる
    bool isPlayer1Win() const {
        int last_player_id = 1 - this->currentPlayerID;
        return (last_player_id == FIRST_PLAYER);
    }


	//  指定したactionでゲームを1ターン進め、次のプレイヤー視点の盤面にする
	void advance(const int action) {
		this->pieces_[action] = 1;
		std::swap(this->pieces_, this->enemy_pieces_);

        //deleteモードのためにキューに追加 //別メソッドがいいかも
        que_transaction.push_back(action);
    
        //人・CPU判定用
        this->currentPlayerID = 1 - this->currentPlayerID;
	}
    
    //プレイヤー入力判定用
    bool isValidAction(const int action) const {
        if (action < 0 || action > 8) return false;
        if (this->pieces_[action] == 1 || this->enemy_pieces_[action] == 1) return false;
        return true;
    }


	//  現在のプレイヤーが可能な行動を全て取得する
	vector<int> legalActions()const {
		vector<int> actions;
		for (int i = 0; i < 9; i++) {
			if (this->pieces_[i] == 0 && this->enemy_pieces_[i] == 0) {
				actions.emplace_back(i);
			}
		}
		return actions;
	}



    //oxを消去する(delete_mode)
    //操作記録のキューの長さが指定以上のときに、先頭から消していく
    //消したのをpiecesにも反映させる
    void deleteCell(){
        if (que_transaction.size() <= MAX_PUT_OX) return;

        //debug
        //cout << "queの長さ : " << que_transaction.size() << endl;

        int number;
        number = que_transaction.front();
        this->enemy_pieces_[number] = 0; //１つ前の人の操作から消すため
        que_transaction.pop_front();
        
        //debug
        //cout << "消した要素 : " << number << endl;
        //cout << "queの長さ : " << que_transaction.size() << endl;
      
    }

    //次消える自分のマスの位置を取得(delete_mode)
    //マスに6つ置いてあるときのみ
    int getNextDelMyPos(){
        return this->que_transaction.front();
    }

    //次消える相手のマスの位置を取得(delete_mode)
    //マスに6つ置いてあるときのみ
    int getNextDelEnemyPos(){
        return this->que_transaction.at(1); //次の相手の消えるマスなので１index
    }

    //指定のインデックスが次消えるマスであるか
    //マスに6つ置いてあるときのみ
    bool isNextDeleteCell(int index){
        return (this->pieceCount(this->pieces_) + this->pieceCount(this->enemy_pieces_)) == MAX_PUT_OX;
    }

    //指定のインデックスのマスにoxが置かれているか
    bool isPutOX(int index){
        return this->pieces_[index] || this->enemy_pieces_[index];
    }

    //盤面のマスに置いているoxの情報をリセット　
    //これを入れてみて、スタート時に、WEB上の盤面がリセットされるかを確認する
    void resetBoard(){
        for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) {
            this->pieces_[i] = 0;
            this->enemy_pieces_[i] = 0;
        }
    } 

    //指定のインデックスのマスに置かれているox
    string getPutOX(int index){
        //outOnCUI()と重複しているので、後で、メソッドとして切り出す
        std::pair<string, string> ox =
			this->isFirstPlayerTurn() ?
			std::pair<string, string>{ "x", "o" } :
			std::pair<string, string>{ "o", "x" };


        if (this->pieces_[index] == 1){
            return ox.first;
        }else{
            return ox.second;
        }

    }

    //現在のゲーム状況表示
	void outOnCUI() const {
		std::stringstream ss;
		std::pair<char, char> ox =
			this->isFirstPlayerTurn() ?
			std::pair<char, char>{ 'x', 'o' } :
			std::pair<char, char>{ 'o', 'x' };
		ss << "player: " << ox.first << std::endl;
		for (int i = 0; i < 9; i++) {
            if (i % 3 == 0)
                ss << "+-+-+-+\n";
            ss <<  "|"; 
			if (this->pieces_[i] == 1)
				ss << ox.first;
			else if (this->enemy_pieces_[i] == 1)
				ss << ox.second;
			else
                ss << to_string(i);
				
			if (i % 3 == 2)
				ss <<  "|\n"; 
		}
        ss << "+-+-+-+\n";
		cout << ss.str() << endl;
	}

};

//*************関数**************************************************

//CUI上でのゲーム設定
void setupGameOnCUI(GameSetup &setup) {
    cout << "対戦形式を選択してください" << endl;
    cout << "0:人間vs人間, 1 : 人間vsCPU(弱), 2 : CPU(強)vs人間, 3 : CPU(強)vsCPU(弱)" << endl;
    int pattern; cin >> pattern;

    cout << "ゲームモードを選択してください " << endl; 
    cout << "0 : 通常, 1 : 消去 " << endl; 
    int mode; cin >> mode;
    setup.game_mode = mode;

    setup.boardSize = BOARD_SIZE;

    if (pattern == 0) {
        setup.player1_isCPU = false;
        setup.player1_CPUstrength = -1;
        setup.player2_isCPU = false;
        setup.player2_CPUstrength = -1;

    } else if (pattern == 1) {
        setup.player1_isCPU = false;
        setup.player1_CPUstrength = -1;
        setup.player2_isCPU = true;
        setup.player2_CPUstrength = CPU_WEAK;

    }else if (pattern == 2) {
        setup.player1_isCPU = true;
        setup.player1_CPUstrength = CPU_STRONG;
        setup.player2_isCPU = false;
        setup.player2_CPUstrength = -1;
    }
    else {
        setup.player1_isCPU = true;
        setup.player1_CPUstrength = CPU_STRONG;
        setup.player2_isCPU = true;
        setup.player2_CPUstrength = CPU_WEAK;
    }
}


//CUI上でのプレイヤー入力
int inputActionOnCUI(const GameState &state) {
    cout << "0 - 8 から入力してください。" << endl;
    int action;  cin >> action;
    return action;
}

//CUI上でのゲーム結果出力
void displayResultOnCUI(const GameState &state){
    cout << "ゲーム終了！" << endl;
    if (state.isDraw()){
        cout << "引き分け" << endl;
    }else if(state.isPlayer1Win()){
        cout << state.getPlayerName(0) << "の勝利！" << endl;
    }else{
        cout << state.getPlayerName(1) << "の勝利！" << endl;
    }
}

//********CPUの行動決定アルゴリズム***********************************
// ランダムに行動を決定する
int randomAction(const GameState &state) {
	auto legal_actions = state.legalActions();
	return legal_actions[mt() % (legal_actions.size())];
}

//αβ
int alphaBetaScore(GameState &state, int alpha, const int beta, const int depth, const TimeKeeper &time_keeper) {
    //if (time_keeper.isTimeOver())return 0;
    if (time_keeper.isTimeOver()){
        return state.getScore();
    }

    if (state.isDone() || depth == 0) {
        return state.getScore();
    }

    auto legal_actions = state.legalActions();
    if (legal_actions.empty()) {
        return state.getScore();
    }
    for (const auto action : legal_actions) {
        GameState next_state = state;
        next_state.advance(action);
        //操作記録取り消し(DELETE_MODEの場合)
        if (state.isDeleteMode()) state.deleteCell();
        int score = -alphaBetaScore(next_state, -beta, -alpha, depth - 1, time_keeper);
        if (time_keeper.isTimeOver())return 0;
        if (score > alpha) {
            alpha = score;
        }
        if (alpha >= beta) {
            return alpha;
        }
    }
    return alpha;
}

int alphaBetaActionWithTimeThreshold(const GameState &state, const int depth, const TimeKeeper& time_keeper) {
    int best_action = -1;
    int alpha = -INF;
    for (const auto action : state.legalActions()) {
        GameState next_state = state;
        next_state.advance(action);
        int score = -alphaBetaScore(next_state, -INF, -alpha, depth, time_keeper);
        if (time_keeper.isTimeOver())return 0;
        if (score > alpha) {
            best_action = action;
            alpha = score;
        }
    }
    return best_action;
}


//ID
int iterativeDeepningAction(const GameState &state, const int64_t time_threshold) {
    auto time_keeper = TimeKeeper(time_threshold);
    int best_action = -1;
    for (int depth = 1;; depth++) {
        int action = alphaBetaActionWithTimeThreshold(state, depth, time_keeper);

        if (time_keeper.isTimeOver()) {
            break;
        }
        else {
            best_action = action;
        }
    }
    return best_action;
}

//TODO 操作記録取り消しの位置について、適切かを確認する
//セグフォが発生したので一旦保留
//MCTS

int argMax(const std::vector<double>& x) {
    return std::distance(x.begin(), std::max_element(x.begin(), x.end()));
}
// ランダムプレイアウトをして勝敗スコアを計算する -> おそらくplayout
double playout(GameState *state) { 
    if (state->isLose())
        return 0;
    if (state->isDraw())
        return 0.5;
    state->advance(randomAction(*state));
    //操作記録取り消し(DELETE_MODEの場合)
    if (state->isDeleteMode()) state->deleteCell(); //->時間制限つけないとダメ。
    return 1. - playout(state);
}

constexpr const double C = 1.; //UCB1の計算に使う定数
constexpr const int EXPAND_THRESHOLD = 10; // ノードを展開する閾値

// MCTSの計算に使うノード
class Node {
private:
    GameState state_;
    double w_;
public:
    std::vector<Node>child_nodes;
    double n_;

	// ノードの評価を行う
    double evaluate() {
        if (this->state_.isDone()) {
            double value = this->state_.isLose() ? 0 : 0.5;
            this->w_ += value;
            ++this->n_;
            return value;
        }
        if (this->child_nodes.empty()) {
            GameState state_copy = this->state_;
            double value = playout(&state_copy);
            this->w_ += value;
            ++this->n_;

            if (this->n_ == EXPAND_THRESHOLD)
                this->expand();

            return value;
        }
        else {
            double value = 1. - this->nextChiledNode().evaluate();
            this->w_ += value;
            ++this->n_;
            return value;
        }
    }

    // ノードを展開する
    void expand() {
        auto legal_actions = this->state_.legalActions();
        this->child_nodes.clear();
        for (const auto action : legal_actions) {
            this->child_nodes.emplace_back(this->state_);
            this->child_nodes.back().state_.advance(action);
        }
    }

    // どのノードを評価するか選択する
    Node& nextChiledNode() {
        for (auto& child_node : this->child_nodes) {
            if (child_node.n_ == 0)
                return child_node;
        }
        double t = 0;
        for (const auto& child_node : this->child_nodes) {
            t += child_node.n_;
        }
        double best_value = -INF;
        int best_i = -1;
        for (int i = 0; i < this->child_nodes.size(); i++) {
            const auto& child_node = this->child_nodes[i];
            double wr = 1. - child_node.w_ / child_node.n_;
            double bias = std::sqrt(2. * std::log(t) / child_node.n_);

            double ucb1_value = 1. - child_node.w_ / child_node.n_ + (double)C * std::sqrt(2. * std::log(t) / child_node.n_);
            if (ucb1_value > best_value) {
                best_i = i;
                best_value = ucb1_value;
            }
        }
        return this->child_nodes[best_i];
    }

    Node(const GameState &state) :state_(state), w_(0), n_(0) {}

};

// 制限時間(ms)を指定してMCTSで行動を決定する
int mctsActionWithTimeThreshold(const GameState &state, const int64_t time_threshold) {
    Node root_node = Node(state);
    root_node.expand();
    auto time_keeper = TimeKeeper(time_threshold);
    for (int cnt = 0;; cnt++) {
        if (time_keeper.isTimeOver()) {
            break;
        }
        root_node.evaluate();
    }
    auto legal_actions = state.legalActions();

    int best_n = -1;
    int best_i = -1;
    assert(legal_actions.size() == root_node.child_nodes.size());
    for (int i = 0; i < legal_actions.size(); i++) {
        int n = root_node.child_nodes[i].n_;
        if (n > best_n) {
            best_i = i;
            best_n = n;
        }
    }
    return legal_actions[best_i];
}



//CPUの行動選択

int calculateAction(const GameState &state) {
    int game_mode = state.getGameMode();
    int cpu_level = state.getCPULevel();

    switch (cpu_level)
    {
    case CPU_WEAK:
        return randomAction(state);
    
    case CPU_STRONG:
        return (state.isDeleteMode())  //MCTS実装後, コメントアウトして、イン
            //? mctsActionWithTimeThreshold(state, CPU_MAX_THINK_TIME)
            ? iterativeDeepningAction(state, CPU_MAX_THINK_TIME)
            :iterativeDeepningAction(state, CPU_MAX_THINK_TIME) ;
    default:
        return randomAction(state);
    }
    
}


//**********************処理フロー*********************** 


//STEP1
//CUI上での処理
int mainCUI(){
   
    //初期化処理
    GameSetup setup;
    setupGameOnCUI(setup);
    GameState state;
    if (state.init(setup) != 0) return -1;
    
    //盤面表示
    state.outOnCUI();

    //ゲームループ
    while(true){

        //プレイヤーの入力またはCPU計算
        int action;
        if (state.isPlayerTurn()) {
            while (true) {
                action = inputActionOnCUI(state);
                if (state.isValidAction(action)) break;
                cout << "入力ミスです。もう一度入力してください。" << endl;
            }
        }
        else {
            int st = clock();
            action = calculateAction(state);
            while (clock() - st < 1 * CLOCKS_PER_SEC);
        }


        //１手進める
        state.advance(action);

        //操作記録取り消し(DELETE_MODEの場合)
        if (state.isDeleteMode()) state.deleteCell();

        //盤面表示
        state.outOnCUI();

        //終了条件
        if (state.isDone()) break;

       
    }

    //結果出力
    displayResultOnCUI(state);

    return 0;
}


//TODO -> STEP2
//pythonでのテスト処理
int mainTest(int game_mode, int cpu1_strength, int cpu2_strength, int test_cnt){


    return 0;
}



//CUI・Python用(テスト用)
#ifndef __EMSCRIPTEN__

//メイン処理
int main(int argc, char *argv[]) {
    //テスト用 .exe(cpp実行ファイル) [game_mode] [cpu1_strength] [cpu2_strength] [testcount]
    if (argc == 5){
        return mainTest(stoi(argv[1]), stoi(argv[2]), stoi(argv[3]), stoi(argv[4]));
    }
    //CUI上での対戦
    return mainCUI();
}

#endif


//TODO -> STEP3
//UI実装時に実装する
//JavaScriptから呼び出されたときの処理
#ifdef __EMSCRIPTEN__
#include <emscripten/bind.h>
GameState JS_state;

//ラッパー関数
//初期化
void JS_initializeGame(int boardsize, int game_mode, bool player1_isCPU, int player1_CPUstrength, bool player2_isCPU, int player2_CPUstrength) {
    GameSetup setup;
    setup.boardSize = boardsize;
    setup.game_mode = game_mode;
    setup.player1_isCPU = player1_isCPU;
    setup.player1_CPUstrength = player1_CPUstrength;
    setup.player2_isCPU = player2_isCPU;
    setup.player2_CPUstrength = player2_CPUstrength;
    JS_state.init(setup);
}

//画面更新

//ボードサイズ取得
int JS_getBoardSize() { return BOARD_SIZE; }

//マスが置かれているか
bool JS_isPutOX(int chipIndex) { return JS_state.isPutOX(chipIndex);}
//指定インデックスに置かれているマス
string JS_getPutOX(int chipIndex) {return JS_state.getPutOX(chipIndex);}

//次消えるマスをハイライトにする 
bool JS_nextDeleteCell(int chipIndex) {
    if (JS_state.isDeleteMode()) {
        return JS_state.isNextDeleteCell(chipIndex);
    }
    return false;
}

//開始時の盤面の初期化
void JS_resetBoard(){return JS_state.resetBoard();}

//現在の手番のプレイヤー名
string JS_getCurrentPlayerString() { return "現在の手番：" + JS_state.getCurrentPlayerName(); }

//終了判定
bool JS_isFinished() { return JS_state.isDone(); }

//思考ループ
//CPUの手番か
bool JS_isCurrentPlayerCPU() { return JS_state.isCurrentPlayerCPU(); }
//合法手であるか
bool JS_isValidAction(int chipIndex) { return JS_state.isValidAction(chipIndex); }
//CPUの計算
int JS_calculateAction() { return calculateAction(JS_state); }
//実行
void JS_advance(int chipIndex) { JS_state.advance(chipIndex); }
//消去
void JS_deleteCell() {if (JS_state.isDeleteMode()) JS_state.deleteCell();}
//ゲーム終了
string JS_getGameOverString() {
    if (JS_state.isDraw()) return "引き分け！";
    return JS_state.getWinnerName() + "の勝ち！";
}

EMSCRIPTEN_BINDINGS(myModule)
{
    emscripten::function("JS_initializeGame", &JS_initializeGame);
    emscripten::function("JS_getBoardSize", &JS_getBoardSize);
    emscripten::function("JS_isPutOX", &JS_isPutOX);
    emscripten::function("JS_getPutOX", &JS_getPutOX);
    emscripten::function("JS_nextDeleteCell", &JS_nextDeleteCell);
    emscripten::function("JS_resetBoard", &JS_resetBoard);
    emscripten::function("JS_getCurrentPlayerString", &JS_getCurrentPlayerString);
    emscripten::function("JS_isFinished", &JS_isFinished);
    emscripten::function("JS_isCurrentPlayerCPU", &JS_isCurrentPlayerCPU);
    emscripten::function("JS_isValidAction", &JS_isValidAction);
    emscripten::function("JS_calculateAction", &JS_calculateAction);
    emscripten::function("JS_advance", &JS_advance);
    emscripten::function("JS_deleteCell", &JS_deleteCell);
    emscripten::function("JS_getGameOverString", &JS_getGameOverString);
    
}
#endif