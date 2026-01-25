#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <condition_variable>

#include "../Search/AntSearch.hpp"

#include "../Eval/NeuroNetEval.hpp"


class Bot {
private:
    Gigantua::Board board;
    Search::Ant::Engine antEngine;
    std::list<uint64_t> history;

public:
    Bot(std::function<float(const Gigantua::Board&)> costFunc) : board(Gigantua::Board::StartPositionFen())
        , antEngine(costFunc, 2000000)
    {
    }

    void NotifyNewGame() {
        history.clear();
        SetPosition(Gigantua::Board::StartPositionFen());
    }

    void SetPosition(const std::string& fen) {
        board = Gigantua::Board(fen); 
        history.clear();
    }

    void MakeMove(const std::string& moveStr) {
        uint8_t from = 0;
        uint8_t to = 0;
        int8_t type = 0;
        Gigantua::Board::moveFromStr(moveStr, from, to, type);
        if (board.status.WhiteMove()) {
            Gigantua::Board::Move<true> moveW;
            const auto nextW = Gigantua::MoveList::MoveList<true>(board);
            for (const auto move : nextW) {
                if (type > 0) {
                    if (from == move.from() && to == move.to() && type == int8_t(move.type())) {
                        moveW = move;
                        break;
                    }
                }
                else {
                    if (from == move.from() && to == move.to()) {
                        moveW = move;
                        break;
                    }
                }
            }
            if (moveW.move) {
                board = moveW.play(board);
                history.push_front(board.Hash);
            }
        }
        else {
            Gigantua::Board::Move<false> moveB;
            const auto nextW = Gigantua::MoveList::MoveList<false>(board);
            for (const auto move : nextW) {
                if (type > 0) {
                    if (from == move.from() && to == move.to() && type == int8_t(move.type())) {
                        moveB = move;
                        break;
                    }
                }
                else {
                    if (from == move.from() && to == move.to()) {
                        moveB = move;
                        break;
                    }
                }
            }
            if (moveB.move) {
                board = moveB.play(board);
                history.push_front(board.Hash);
            }
        }
    }

    void ThinkTimed(int timeMs) {
 
        std::mutex mtx;
        std::condition_variable cv;
        uint16_t winMove = 0;
        bool done = false;
        std::function<void(uint16_t)> onDone = [&mtx, &cv, &winMove, &done](uint16_t move) {
            winMove = move;
            done = true;
            cv.notify_one();
        };

        antEngine.Set(board);
        std::array<uint64_t, 8> h = { 0 };
        {
            uint8_t i = 0;
            for (uint64_t hash : history) {
                h[i++] = hash;
                if (i + 1 >= h.size()) break;
            }
        }
        antEngine.SetHistory(h);
        antEngine.Start(4, 4, timeMs, onDone);

        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_for(lock, std::chrono::milliseconds(timeMs), [&done] { return done; });

        antEngine.Stop();
        

        const uint16_t bestMove = winMove ? winMove : antEngine.BestMove();

        if (bestMove) {
            std::cout << "bestmove " << Gigantua::Board::moveStr(bestMove) << std::endl;
        }
        else {
            return;
        }

        if (board.status.WhiteMove()) {
            Gigantua::Board::Move<true> m(bestMove);
            antEngine.Set(m.play(board));
        }
        else {
            Gigantua::Board::Move<false> m(bestMove);
            antEngine.Set(m.play(board));
        }

        antEngine.Start(8, 1, 60000, nullptr);
    }
    
    void StopThinking() {
        antEngine.Stop();
    }

    void Quit() {
        StopThinking();
    }

    int ChooseThinkTime(int timeRemainingWhiteMs, int timeRemainingBlackMs, int incrementWhiteMs, int incrementBlackMs) {
        int myTimeRemainingMs = board.status.WhiteMove() ? timeRemainingWhiteMs : timeRemainingBlackMs;
        int myIncrementMs = board.status.WhiteMove() ? incrementWhiteMs : incrementBlackMs;

        const int maxThinkTimeMs = 10000;

        if (myTimeRemainingMs < 0) return maxThinkTimeMs;

        // Get a fraction of remaining time to use for current move
        double thinkTimeMs = myTimeRemainingMs / 40.0;


        // Add increment
        if (myTimeRemainingMs > myIncrementMs * 2) {
            thinkTimeMs += myIncrementMs * 0.75;
        }

        double minThinkTime = std::min(50.0, myTimeRemainingMs * 0.15);
        return static_cast<int>(std::ceil(std::max(minThinkTime, thinkTimeMs)));
    }
    
    std::string GetBoardDiagram() {
        return board.Diagram();
    }
};

class EngineUCI {
private:
    Bot player;
    static std::vector<std::string> positionLabels;
    static std::vector<std::string> goLabels;

public:
    EngineUCI(std::function<float(const Gigantua::Board&)> costFunc) : player(costFunc) {
    }

    void ReceiveCommand(const std::string& message) {
        std::string trimmedMessage = message;
        std::string messageType = trimmedMessage.substr(0, trimmedMessage.find(' '));
        transform(messageType.begin(), messageType.end(), messageType.begin(), ::tolower);

        if (messageType == "uci") Respond("uciok");
        else if (messageType == "isready") Respond("readyok");
        else if (messageType == "ucinewgame") player.NotifyNewGame();
        else if (messageType == "position") ProcessPositionCommand(trimmedMessage);
        else if (messageType == "go") ProcessGoCommand(trimmedMessage);
        else if (messageType == "stop") player.StopThinking();
        else if (messageType == "quit") player.Quit();
        else if (messageType == "d") std::cout << player.GetBoardDiagram() << std::endl;
    }

private:

    void ProcessPositionCommand(const std::string& message) {
        if (message.find("startpos") != std::string::npos) {
            player.SetPosition(Gigantua::Board::StartPositionFen());
        }
        else if (message.find("fen") != std::string::npos) {
            std::string customFen = TryGetLabelledValue(message, "fen", positionLabels);
            player.SetPosition(customFen);
        }

        std::string moves = TryGetLabelledValue(message, "moves", positionLabels);
        if (!moves.empty()) {
            std::istringstream iss(moves);
            std::string move;

            while (iss >> move) {
                move.erase(std::remove(move.begin(), move.end(), '='), move.end());
                player.MakeMove(move);
            }
        }
    }

    void ProcessGoCommand(const std::string& message) {
        int thinkTime = player.ChooseThinkTime(
            TryGetLabelledValueInt(message, "wtime", goLabels),
            TryGetLabelledValueInt(message, "btime", goLabels),
            TryGetLabelledValueInt(message, "winc", goLabels),
            TryGetLabelledValueInt(message, "binc", goLabels)
        );
        player.ThinkTimed(thinkTime);
    }

    int TryGetLabelledValueInt(const std::string& text, const std::string& label, const std::vector<std::string>& allLabels, int defaultValue = -1)
    {
        std::string valueString = TryGetLabelledValue(text, label, allLabels, std::to_string(defaultValue));

        valueString.erase(0, valueString.find_first_not_of(" \t\n\r"));
        valueString.erase(valueString.find_last_not_of(" \t\n\r") + 1);

        // Extract the first word and attempt to convert it to an integer
        size_t firstSpace = valueString.find(' ');
        std::string firstWord = (firstSpace != std::string::npos) ? valueString.substr(0, firstSpace) : valueString;

        try
        {
            return std::stoi(firstWord);
        }
        catch (const std::invalid_argument&)
        {
            return defaultValue;
        }
    }

    std::string TryGetLabelledValue(const std::string& text, const std::string& label, const std::vector<std::string>& allLabels, const std::string& defaultValue = "")
    {
        std::string trimmedText = text;
        trimmedText.erase(0, trimmedText.find_first_not_of(" \t\n\r"));
        trimmedText.erase(trimmedText.find_last_not_of(" \t\n\r") + 1);

        size_t valueStart = trimmedText.find(label);
        if (valueStart != std::string::npos)
        {
            valueStart += label.length();
            size_t valueEnd = trimmedText.length();

            for (const std::string& otherID : allLabels)
            {
                if (otherID != label)
                {
                    size_t otherIDStartIndex = trimmedText.find(otherID);
                    if (otherIDStartIndex != std::string::npos && otherIDStartIndex > valueStart && otherIDStartIndex < valueEnd)
                    {
                        valueEnd = otherIDStartIndex;
                    }
                }
            }

            return trimmedText.substr(valueStart, valueEnd - valueStart);
        }

        return defaultValue;
    }

    void Respond(const std::string& response) {
        std::cout << response << std::endl;
    }
};

std::vector<std::string> EngineUCI::positionLabels = { "position", "fen", "moves" };
std::vector<std::string> EngineUCI::goLabels = { "go", "movetime", "wtime", "btime", "winc", "binc", "movestogo" };

std::vector<float> importNet(const std::string& fileName) {
    std::vector<float> genome;
    std::fstream file;
    file.open(fileName, std::ios::in);

    if (file.is_open()) {
        std::string line;
        size_t i = 0;
        while (std::getline(file, line)) {
            genome.push_back(std::stof(line));
        }

        file.close();
    }

    return genome;
}

int main() {
    const std::vector<float> gen = importNet("genome.txt");
    NN::NeuroNetEval::SetGenome(gen);
   
    std::function<float(const Gigantua::Board&)> costFunc = [](const Gigantua::Board& pos) {
        return NN::NeuroNetEval::Evaluate(pos);
     };

    EngineUCI engine(costFunc);
    std::string command;

    std::ofstream log("log.txt", std::ios::app);

    do {
        std::getline(std::cin, command);
        log << command << "\n";
        if (command.length() > 1)
            engine.ReceiveCommand(command);
    } while (command != "quit");

    return 0;

}